# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import argparse
import os
import re
import subprocess


# This script extracts commits that touch third party webrtc code so they can
# be imported into Git. It filters out commits that are not part of upstream
# code and rewrites the paths to match upstream. Finally, the commits are
# combined into a mailbox file that can be applied with `git am`.
LIBWEBRTC_DIR = "third_party/libwebrtc"


def build_commit_list(revset, env):
    """Build commit list from the specified revset.

    The revset can be a single revision, e.g. 52bb9bb94661, or a range,
    e.g. 8c08a5bb8a99::52bb9bb94661, or any other valid revset
    (check hg help revset). Only commits that touch libwebrtc are included.
    """
    res = subprocess.run(
        ["hg", "log", "-r", revset, "-M", "--template", "{node}\n", LIBWEBRTC_DIR],
        capture_output=True,
        text=True,
        env=env,
        cwd="../../../../",
    )
    return [line.strip() for line in res.stdout.strip().split("\n")]


def extract_author_date(sha1, env):
    res = subprocess.run(
        ["hg", "log", "-r", sha1, "--template", "{author}|{date|isodate}"],
        capture_output=True,
        text=True,
        env=env,
    )
    return res.stdout.split("|")


def extract_description(sha1, env):
    res = subprocess.run(
        ["hg", "log", "-r", sha1, "--template", "{desc}"],
        capture_output=True,
        text=True,
        env=env,
    )
    return res.stdout


def extract_commit(sha1, env):
    res = subprocess.run(
        ["hg", "log", "-r", sha1, "-pg", "--template", "\n"],
        capture_output=True,
        text=True,
        env=env,
    )
    return res.stdout


def filter_nonwebrtc(commit):
    filtered = []
    skipping = False
    for line in commit.split("\n"):
        # Extract only patches affecting libwebrtc, but avoid commits that
        # touch build, which is tracked by a separate repo, or that affect
        # moz.build files which are code generated.
        if (
            line.startswith("diff --git a/" + LIBWEBRTC_DIR)
            and not line.startswith("diff --git a/" + LIBWEBRTC_DIR + "/build")
            and not line.startswith("diff --git a/" + LIBWEBRTC_DIR + "/third_party")
            and not line.endswith("moz.build")
        ):
            skipping = False
        elif line.startswith("diff --git"):
            skipping = True

        if not skipping:
            filtered.append(line)
    return "\n".join(filtered)


def fixup_paths(commit):
    # make sure we only rewrite paths in the diff-related lines
    return re.sub("( [ab])/" + LIBWEBRTC_DIR + "/", "\\1/", commit)


def write_as_mbox(sha1, author, date, description, commit, ofile):
    # Use same magic date as git format-patch
    ofile.write("From {} Mon Sep 17 00:00:00 2001\n".format(sha1))
    ofile.write("From: {}\n".format(author))
    ofile.write("Date: {}\n".format(date))
    description = description.split("\n")
    ofile.write("Subject: {}\n".format(description[0]))
    ofile.write("\n".join(description[1:]))
    ofile.write(
        "\nMercurial Revision: https://hg.mozilla.org/mozilla-central/rev/{}\n".format(
            sha1
        )
    )
    ofile.write(commit)
    ofile.write("\n")
    ofile.write("\n")


if __name__ == "__main__":
    commits = []
    parser = argparse.ArgumentParser(
        description="Format commits for upstream libwebrtc"
    )
    parser.add_argument(
        "revsets", metavar="revset", type=str, nargs="+", help="A revset to process"
    )
    args = parser.parse_args()

    # must run 'hg' with HGPLAIN=1 to ensure aliases don't interfere with
    # command output.
    env = os.environ.copy()
    env["HGPLAIN"] = "1"

    for revset in args.revsets:
        commits.extend(build_commit_list(revset, env))

    with open("mailbox.patch", "w") as ofile:
        for sha1 in commits:
            author, date = extract_author_date(sha1, env)
            description = extract_description(sha1, env)
            filtered_commit = filter_nonwebrtc(extract_commit(sha1, env))
            if len(filtered_commit) == 0:
                continue
            fixedup_commit = fixup_paths(filtered_commit)
            write_as_mbox(sha1, author, date, description, fixedup_commit, ofile)
