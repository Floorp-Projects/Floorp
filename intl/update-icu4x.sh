#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

# Update the icu4x binary data for a given release:
#   Usage: update-icu4x.sh <URL of ICU GIT> <release tag name> <CLDR version> <ICU release tag name>
#   update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.2.0 43.0.0 release-73-1
#
# Update to the main branch:
#   Usage: update-icu4x.sh <URL of ICU GIT> <branch> <CLDR version> <ICU release tag name>
#   update-icu4x.sh https://github.com/unicode-org/icu4x.git main 43.0.0 release-73-1

# default
cldr=${3:-43.0.0}
icuexport=${4:-release-73-1}

if [ $# -lt 2 ]; then
  echo "Usage: update-icu4x.sh <URL of ICU4X GIT> <ICU4X release tag name> <CLDR version> <ICU release tag name>"
  echo "Example: update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.2.0 43.0.0 release-73-1"
  exit 1
fi

# Make a log function so the output is easy to read.
log() {
  CYAN='\033[0;36m'
  CLEAR='\033[0m'
  printf "${CYAN}[update-icu4x]${CLEAR} $*\n"
}

# Specify locale and time zone information for consistent output and reproduceability.
export TZ=UTC
export LANG=en_US.UTF-8
export LANGUAGE=en_US
export LC_ALL=en_US.UTF-8

# Define all of the paths.
original_pwd=$(pwd)
top_src_dir=$(cd -- "$(dirname "$0")/.." >/dev/null 2>&1 ; pwd -P)
data_dir=${top_src_dir}/intl/icu_testdata/data/baked
git_info_file=${data_dir}/ICU4X-GIT-INFO

log "Remove the old data"
rm -rf ${data_dir}

log "Clone ICU4X"
tmpclonedir=$(mktemp -d)
git clone --depth 1 --branch $2 $1 ${tmpclonedir}

log "Change the directory to the cloned repo"
log ${tmpclonedir}
cd ${tmpclonedir}

log "Patching line segmenter data to fix https://github.com/unicode-org/icu4x/issues/3811."
# This manually patch can be removed once we upgrade to ICU4X 1.3 in bug 1851323.
wget --unlink -q -O ${tmpclonedir}/provider/datagen/data/segmenter/line.toml https://raw.githubusercontent.com/unicode-org/icu4x/fd62de5e232ea1f0cb81e88dc6eb0cf9c86f85ca/provider/datagen/src/transform/segmenter/rules/line.toml

log "Run the icu4x-datagen tool to regenerate the data."
log "Saving the data into: ${data_dir}"

# TODO(Bug 1741262) - Should locales be filtered as well? It doesn't appear that the existing ICU
# data builder is using any locale filtering.

# --keys <KEYS>...
#     Include this resource key in the output. Accepts multiple arguments.
# --key-file <KEY_FILE>
#     Path to text file with resource keys to include, one per line. Empty lines and
#     lines starting with '#' are ignored.
cargo run --bin icu4x-datagen          \
  --features=bin                       \
  --                                   \
  --cldr-tag ${cldr}                   \
  --icuexport-tag ${icuexport}         \
  --keys segmenter/dictionary/w_auto@1 \
  --keys segmenter/grapheme@1          \
  --keys segmenter/line@1              \
  --keys segmenter/lstm/wl_auto@1      \
  --keys segmenter/sentence@1          \
  --keys segmenter/word@1              \
  --all-locales                        \
  --use-separate-crates                \
  --format mod                         \
  --out ${data_dir}                    \

log "Record the current cloned git information to:"
log ${git_info_file}
# (This ensures that if ICU modifications are performed properly, it's always
# possible to run the command at the top of this script and make no changes to
# the tree.)
git -C ${tmpclonedir} log -1 > ${git_info_file}

log "Clean up the tmp directory"
cd ${original_pwd}
rm -rf ${tmpclonedir}
