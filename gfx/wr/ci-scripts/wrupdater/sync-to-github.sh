#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# Do NOT set -x here, since that will expose a secret API token!
set -o errexit
set -o nounset
set -o pipefail

if [[ "$(uname)" != "Linux" ]]; then
    echo "Error: this script must be run on Linux due to readlink semantics"
    exit 1
fi

# GECKO_PATH should definitely be set
if [[ -z "${GECKO_PATH}" ]]; then
    echo "Error: GECKO_PATH must point to a hg clone of mozilla-central"
    exit 1
fi

# Internal variables, don't fiddle with these
MYSELF=$(readlink -f ${0})
MYDIR=$(dirname "${MYSELF}")
WORKDIR="${HOME}/.wrupdater"
TMPDIR="${WORKDIR}/tmp"
SECRET="project/webrender-ci/wrupdater-github-token"
MOZ_SCM_LEVEL=${MOZ_SCM_LEVEL:-1} # 1 means try push, so no access to secret

mkdir -p "${TMPDIR}"

# Bring the webrender clone to a known good up-to-date state
if [[ ! -d "${WORKDIR}/webrender" ]]; then
    echo "Setting up webrender repo..."
    git clone https://github.com/servo/webrender "${WORKDIR}/webrender"
    pushd "${WORKDIR}/webrender"
    git remote add moz-gfx https://github.com/moz-gfx/webrender
    popd
else
    echo "Updating webrender repo..."
    pushd "${WORKDIR}/webrender"
    git checkout master
    git pull
    popd
fi

if [[ "${MOZ_SCM_LEVEL}" != "1" ]]; then
    echo "Obtaining github API token..."
    # Be careful, GITHUB_TOKEN is secret, so don't log it (or any variables
    # built using it).
    GITHUB_TOKEN=$(
        curl -sSfL "http://taskcluster/secrets/v1/secret/${SECRET}" |
        ${MYDIR}/read-json.py "secret/token"
    )
    AUTH="moz-gfx:${GITHUB_TOKEN}"
fi

echo "Pushing base wrupdater branch..."
pushd "${WORKDIR}/webrender"
git fetch moz-gfx
git checkout -B wrupdater moz-gfx/wrupdater || git checkout -B wrupdater master

if [[ "${MOZ_SCM_LEVEL}" != "1" ]]; then
    # git may emit error messages that contain the URL, so let's sanitize them
    # or we might leak the auth token to the task log.
    git push "https://${AUTH}@github.com/moz-gfx/webrender" \
        wrupdater:wrupdater 2>&1 | sed -e "s/${AUTH}/_SANITIZED_/g"
fi
popd

# Run the converter
echo "Running converter..."
pushd "${GECKO_PATH}"
"${MYDIR}/converter.py" "${WORKDIR}/webrender"
popd

# Check to see if we have changes that need pushing
echo "Checking for new changes..."
pushd "${WORKDIR}/webrender"
PATCHCOUNT=$(git log --oneline moz-gfx/wrupdater..wrupdater | wc -l)
if [[ ${PATCHCOUNT} -eq 0 ]]; then
    echo "No new patches found, aborting..."
    exit 0
fi

# Log the new changes, just for logging purposes
echo "Here are the new changes:"
git log --graph --stat moz-gfx/wrupdater..wrupdater

# Collect PR numbers of PRs opened on Github and merged to m-c
set +e
FIXES=$(
    git log master..wrupdater |
    grep "\[import_pr\] From https://github.com/servo/webrender/pull" |
    sed -e "s%.*pull/% Fixes #%" |
    uniq |
    tr '\n' ','
)
echo "${FIXES}"
set -e

if [[ "${MOZ_SCM_LEVEL}" == "1" ]]; then
    echo "Running in try push, exiting now"
    exit 0
fi

echo "Pushing new changes to moz-gfx..."
# git may emit error messages that contain the URL, so let's sanitize them
# or we might leak the auth token to the task log.
git push "https://${AUTH}@github.com/moz-gfx/webrender" +wrupdater:wrupdater \
    2>&1 | sed -e "s/${AUTH}/_SANITIZED_/g"

CURL_HEADER="Accept: application/vnd.github.v3+json"
CURL=(curl -sSfL -H "${CURL_HEADER}" -u "${AUTH}")
# URL extracted here mostly to make servo-tidy happy with line lengths
API_URL="https://api.github.com/repos/servo/webrender"

# Check if there's an existing PR open
echo "Listing pre-existing pull requests..."
"${CURL[@]}" "${API_URL}/pulls?head=moz-gfx:wrupdater" |
    tee "${TMPDIR}/pr.get"
set +e
COMMENT_URL=$(cat "${TMPDIR}/pr.get" | ${MYDIR}/read-json.py "0/comments_url")
HAS_COMMENT_URL="${?}"
set -e

if [[ ${HAS_COMMENT_URL} -ne 0 ]]; then
    echo "Pull request not found, creating..."
    # The PR doesn't exist yet, so let's create it
    (   echo -n '{ "title": "Sync changes from mozilla-central"'
        echo -n ', "body": "'"${FIXES}"'"'
        echo -n ', "head": "moz-gfx:wrupdater"'
        echo -n ', "base": "master" }'
    ) > "${TMPDIR}/pr.create"
    "${CURL[@]}" -d "@${TMPDIR}/pr.create" "${API_URL}/pulls" |
        tee "${TMPDIR}/pr.response"
    COMMENT_URL=$(
        cat "${TMPDIR}/pr.response" |
        ${MYDIR}/read-json.py "comments_url"
    )
fi

# At this point COMMENTS_URL should be set, so leave a comment to tell bors
# to merge the PR.
echo "Posting r+ comment to ${COMMENT_URL}..."
echo '{ "body": "@bors-servo r+" }' > "${TMPDIR}/bors_rplus"
"${CURL[@]}" -d "@${TMPDIR}/bors_rplus" "${COMMENT_URL}"

echo "All done!"
