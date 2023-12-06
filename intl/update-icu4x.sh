#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

# Update the icu4x binary data for a given release:
#   Usage: update-icu4x.sh <URL of ICU GIT> <release tag name> <CLDR version> <ICU release tag name> <ICU4X version of icu_capi>
#   update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.4.0 44.0.0 release-74-1 1.4.0
#
# Update to the main branch:
#   Usage: update-icu4x.sh <URL of ICU GIT> <branch> <CLDR version> <ICU release tag name> <ICU4X version of icu_capi>
#   update-icu4x.sh https://github.com/unicode-org/icu4x.git main 44.0.0 release-74-1 1.4.0

# default
cldr=${3:-44.0.0}
icuexport=${4:-release-74-1}
icu4x_version=${5:-1.4.0}

if [ $# -lt 2 ]; then
  echo "Usage: update-icu4x.sh <URL of ICU4X GIT> <ICU4X release tag name> <CLDR version> <ICU release tag name> <ICU4X version for icu_capi>"
  echo "Example: update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.4.0 44.0.0 release-74-1 1.4.0"
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
segmenter_data_dir=${top_src_dir}/intl/icu_segmenter_data/data
git_info_file=${segmenter_data_dir}/ICU4X-GIT-INFO

log "Remove the old data"
rm -rf ${segmenter_data_dir}

log "Download icuexportdata"
tmpicuexportdir=$(mktemp -d)
icuexport_filename=`echo "icuexportdata_${icuexport}.zip" | sed "s/\//-/g"`
cd ${tmpicuexportdir}
wget https://github.com/unicode-org/icu/releases/download/${icuexport}/${icuexport_filename}

log "Patching icuexportdata to reduce data size"
unzip ${icuexport_filename}
for toml in          \
    burmesedict.toml \
    khmerdict.toml   \
    laodict.toml     \
    thaidict.toml    \
; do
    cp ${top_src_dir}/intl/icu4x-patches/empty.toml ${tmpicuexportdir}/segmenter/dictionary/$toml
done

log "Clone ICU4X"
tmpclonedir=$(mktemp -d)
git clone --depth 1 --branch $2 $1 ${tmpclonedir}

log "Change the directory to the cloned repo"
log ${tmpclonedir}
cd ${tmpclonedir}

log "Copy icu_capi crate to local since we need a patched version"
rm -rf ${top_src_dir}/intl/icu_capi
wget -O icu_capi.tar.gz https://crates.io/api/v1/crates/icu_capi/${icu4x_version}/download
tar xf icu_capi.tar.gz -C ${top_src_dir}/intl
mv ${top_src_dir}/intl/icu_capi-${icu4x_version} ${top_src_dir}/intl/icu_capi
rm -rf icu_capi_tar.gz

log "Patching icu_capi"
for patch in \
    001-Cargo.toml.patch \
    002-GH4109.patch \
    003-explicit.patch \
; do
    patch -d ${top_src_dir} -p1 --no-backup-if-mismatch < ${top_src_dir}/intl/icu4x-patches/$patch
done

# ICU4X 1.3 or later with icu_capi uses each compiled_data crate.

log "Run the icu4x-datagen tool to regenerate the segmenter data."
log "Saving the data into: ${segmenter_data_dir}"

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
  --icuexport-root ${tmpicuexportdir}  \
  --keys segmenter/dictionary/w_auto@1 \
  --keys segmenter/dictionary/wl_ext@1 \
  --keys segmenter/grapheme@1          \
  --keys segmenter/line@1              \
  --keys segmenter/lstm/wl_auto@1      \
  --keys segmenter/sentence@1          \
  --keys segmenter/word@1              \
  --all-locales                        \
  --format mod                         \
  --out ${segmenter_data_dir}          \

log "Record the current cloned git information to:"
log ${git_info_file}
# (This ensures that if ICU modifications are performed properly, it's always
# possible to run the command at the top of this script and make no changes to
# the tree.)
git -C ${tmpclonedir} log -1 > ${git_info_file}

log "Clean up the tmp directory"
cd ${original_pwd}
rm -rf ${tmpclonedir}
rm -rf ${tmpicuexportdir}
