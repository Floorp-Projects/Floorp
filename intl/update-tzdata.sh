#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

# Usage: update-tzdata.sh <tzdata version>
# E.g., for tzdata2016f: update-tzdata.sh 2016f

# Ensure that $Date$ in the checked-out svn files expands timezone-agnostically,
# so that this script's behavior is consistent when run from any time zone.
export TZ=UTC

# Also ensure SVN-INFO is consistently English.
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

# Path to icupkg executable, typically located at $ICU_DIR/bin/icupkg.
icu_pkg=
# Force updates even when current tzdata is newer than the requested version.
force=false
# Dry run, doesn't run 'svn export' and 'icupkg'.
dry=false
# Compare ICU and local tzdata versions (used by update-icu.sh).
check_version=false

while getopts ce:fd opt
do
    case "$opt" in
      c) check_version=true;;
      e) icu_pkg="$OPTARG";;
      f) force=true;;
      d) dry=true;;
      \?)
        echo >&2 "Usage: $0 [-e <path to icupkg>] [-f] [-d] <tzdata version>"
        exit 1;;
    esac
done
shift "$((OPTIND - 1))"

if [ $# -ne 1 -a $check_version = false ]; then
  echo >&2 "Usage: $0 [-e <path to icupkg>] [-f] [-d] <tzdata version>"
  exit 1
fi

tzdata_version=$1

icudata_dir=`dirname "$0"`/../config/external/icu/data
icu_dir=`dirname "$0"`/icu
tzdata_dir=`dirname "$0"`/tzdata
tzdata_files="${tzdata_dir}"/files.txt
tzdata_url=https://ssl.icu-project.org/repos/icu/data/trunk/tzdata/icunew/${tzdata_version}/44/
icu_tzdata_version=`grep --only-matching --perl-regexp --regexp="tz version:\s+\K.*$" "${icu_dir}"/source/data/misc/zoneinfo64.txt`
local_tzdata_version=
if [ -f "${tzdata_dir}"/SVN-INFO ]; then
  local_tzdata_version=`grep --only-matching --perl-regexp --regexp="^URL: .*tzdata/icunew/\K[0-9a-z]+" "${tzdata_dir}"/SVN-INFO`
fi

# Check ICU and current local tzdata versions.
if [ $check_version = true ]; then
  if [ ! -z ${local_tzdata_version} ]; then
    if [ ${local_tzdata_version} \> ${icu_tzdata_version} ]; then
      echo >&2 "WARN: Local tzdata (${local_tzdata_version}) is newer than ICU tzdata (${icu_tzdata_version}), please run '$0 ${local_tzdata_version}'"
    else
      echo "INFO: ICU tzdata ${icu_tzdata_version} is newer than local tzdata (${local_tzdata_version})"
    fi
  else
    echo "INFO: No local tzdata files found"
  fi
  exit 0
fi

# Find icu_pkg if not provided as an argument.
icu_pkg=${icu_pkg:-`which icupkg`}

# Test if we can execute icupkg.
if [ ! -x "${icu_pkg}" ]; then
  echo >&2 "ERROR: icupkg is not an executable"
  exit 1
fi

# Check ICU tzdata version.
if [ ${icu_tzdata_version} \> ${tzdata_version} ]; then
  if [ $force = false ]; then
    echo >&2 "ERROR: ICU tzdata (${icu_tzdata_version}) is newer than requested version ${tzdata_version}, use -f to force replacing"
    exit 1
  fi
fi

# Check tzdata version from last checkout.
if [ -n ${local_tzdata_version} -a ${local_tzdata_version} \> ${tzdata_version} ]; then
  if [ $force = false ]; then
    echo >&2 "ERROR: Local tzdata (${local_tzdata_version}) is newer than requested version ${tzdata_version}, use -f to force replacing"
    exit 1
  fi
fi

echo "INFO: Updating tzdata from ${local_tzdata_version:-$icu_tzdata_version} to ${tzdata_version}"

# Search for ICU data files.
# Little endian data files.
icudata_file_le=`find "${icudata_dir}" -type f -name 'icudt*l.dat'`
if [ -f "${icudata_file_le}" ]; then
  icudata_file_le=`cd "$(dirname "${icudata_file_le}")" && pwd -P`/`basename "${icudata_file_le}"`
  echo "INFO: ICU data file (little endian): ${icudata_file_le}"
else
  echo >&2 "ERROR: ICU data (little endian) file not found"
  exit 1
fi

# Big endian data files.
# Optional until https://bugzilla.mozilla.org/show_bug.cgi?id=1264836 is fixed.
icudata_file_be=`find "${icudata_dir}" -type f -name 'icudt*b.dat'`
if [ -f "${icudata_file_be}" ]; then
  icudata_file_be=`cd "$(dirname "${icudata_file_be}")" && pwd -P`/`basename "${icudata_file_be}"`
  echo "INFO: ICU data file (big endian): ${icudata_file_be}"
else
  echo "INFO: ICU data file (big endian) not found, skipping..."
fi

# Retrieve tzdata from svn.
if [ $dry = false ]; then
  echo "INFO: Downloading tzdata${tzdata_version}"

  # Remove intl/tzdata/source, then replace it with a clean export.
  rm -r "${tzdata_dir}"/source
  svn export "${tzdata_url}" "${tzdata_dir}"/source
fi

# Record `svn info`, eliding the line that changes every time the entire ICU
# tzdata repository (not just the path within it we care about) receives a
# commit.
if [ $dry = false ]; then
  svn info "${tzdata_url}" | grep --invert-match '^Revision: [[:digit:]]\+$' > "${tzdata_dir}"/SVN-INFO
fi

# Update ICU data.
update_icu_data() {
  set +e

  local type="$1"
  local file="$2"
  local cmd="${icu_pkg} --add ${tzdata_files} --sourcedir ${tzdata_dir}/source/${type} ${file}"
  eval "${cmd}"

  local exit_status=$?

  if [ $exit_status -ne 0 ]; then
    echo >&2 "ERROR: Error updating tzdata"
    echo >&2 "ERROR: If you see an error message like 'format version 03.00 not supported',\n"\
              "      ensure your icupkg version matches the current ICU version."
    exit $exit_status
  fi

  set -e
}

if [ $dry = false ]; then
  update_icu_data "le" "${icudata_file_le}"
  if [ -n "${icudata_file_be}" ]; then
    update_icu_data "be" "${icudata_file_be}"
  fi

  hg addremove "${tzdata_dir}/source" "${tzdata_dir}/SVN-INFO" "${icudata_file_le}"
  if [ -n "${icudata_file_be}" ]; then
    hg addremove "${icudata_file_be}"
  fi

  echo "INFO: Successfully updated tzdata!"
  echo "INFO: Please run js/src/builtin/make_intl_data.py to update additional time zone files for SpiderMonkey."
fi
