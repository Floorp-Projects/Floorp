#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
set -e

# Arguments:
#   $1: platform (linux|mac|windows)
#   $2: arch (x86_64|aarch64)
#   $3: debug (true|false)
#   $4: pgo (true|false)
#   $5: pgo_mode ("generate"|"use")
#   $6: pgo_artifact_name (string, for "use" mode)
#   $7: MOZ_BUILD_DATE (optional)

if (( $# < 4 || $# > 7 )); then
  echo "Expected platform, arch, debug, pgo, optional mode, profile and build date" >&2
  exit 2
fi

PLATFORM="$1"
ARCH="$2"
DEBUG="$3"
PGO="$4"
PGO_MODE="${5:-}"
PGO_ARTIFACT_NAME="${6:-}"
MOZ_BUILD_DATE="${7:-}"

PROFILE_ARGS=(
  --platform "$PLATFORM" --arch "$ARCH" --debug "$DEBUG" --pgo "$PGO"
  --mode "$PGO_MODE" --artifact "$PGO_ARTIFACT_NAME"
)
if [[ "$DEBUG" == "true" && "$PGO" == "true" ]]; then
  PROFILE_ARGS+=(--allow-legacy-debug-pgo)
fi
python3 "$GITHUB_WORKSPACE/.github/workflows/scripts/qa3_build_profile.py" \
  --validate-inputs "${PROFILE_ARGS[@]}"

if [[ -n "$MOZ_BUILD_DATE" ]]; then
  export MOZ_BUILD_DATE="$MOZ_BUILD_DATE"
fi

cd "$GITHUB_WORKSPACE"

# Apply debug patches if in debug mode
if [[ "$DEBUG" == "true" ]]; then
  PATCH_DIR=".github/patches/debug"
  if [ -d "$PATCH_DIR" ]; then
    echo "Applying debug patches..."
    for patch in "$PATCH_DIR"/*.patch; do
      [ -e "$patch" ] || continue
      echo "Applying debug patch: $(basename "$patch")"
      git apply --verbose --ignore-space-change --ignore-whitespace "$patch"
    done
  else
    echo "No debug patches to apply"
  fi
fi

if [[ "$PLATFORM" == "windows" ]]; then
  cp ./.github/workflows/mozconfigs/windows-x86_64.mozconfig mozconfig
elif [[ "$PLATFORM" == "linux" ]]; then
  if [[ "$ARCH" == "aarch64" ]]; then
    cp ./.github/workflows/mozconfigs/linux-aarch64.mozconfig mozconfig
  else
    cp ./.github/workflows/mozconfigs/linux-x86_64.mozconfig mozconfig
  fi
elif [[ "$PLATFORM" == "mac" ]]; then
  if [[ "$ARCH" == "x86_64" ]]; then
    cp ./.github/workflows/mozconfigs/macosx64-x86_64.mozconfig mozconfig
  else
    cp ./.github/workflows/mozconfigs/macosx64-aarch64.mozconfig mozconfig
  fi

  # Add macOS SDK path for cross-compilation
  echo "ac_add_options --with-macos-sdk=$(echo ~)/macos-sdk" >> mozconfig
fi

# Copy branding assets
if [[ -d ".github/assets/branding" ]]; then
  cp -r ./.github/assets/branding/* ./browser/branding/

  # Select branding based on debug mode
  if [[ "$DEBUG" == "true" ]]; then
    # Debug mode: use unofficial branding
    BRANDING="browser/branding/floorp-daylight"
    echo "Using debug branding: floorp-daylight"
  else
    # Release mode: use official branding
    BRANDING="browser/branding/floorp-official"
    echo "Using official branding: floorp-official"
  fi

  echo "ac_add_options --with-branding=$BRANDING" >> mozconfig

  # Set Flat Chrome (skip for profile generation)
  if [[ "$PGO_MODE" != "generate" ]]; then
    echo "ac_add_options --enable-chrome-format=flat" >> mozconfig
  fi
else
  echo "No custom branding found, using default Firefox branding"
fi

# Enable Linker for Mac
if [[ "$PLATFORM" == "mac" ]]; then
  echo "ac_add_options --enable-linker=lld" >> mozconfig
fi

SCCACHE_BIN="${SCCACHE_PATH:-}"
if [[ -z "$SCCACHE_BIN" ]]; then
  SCCACHE_BIN="$(command -v sccache || true)"
fi

if [[ -n "$SCCACHE_BIN" ]]; then
  # Honor the installed sccache path instead of assuming a fixed toolcache location.
  {
    echo "mk_add_options 'export RUSTC_WRAPPER=${SCCACHE_BIN}'"
    echo "mk_add_options 'export CCACHE_CPP2=yes'"
    echo "ac_add_options --with-ccache=${SCCACHE_BIN}"
    echo "mk_add_options 'export SCCACHE_GHA_ENABLED=on'"
  } >> mozconfig
else
  echo "Warning: sccache was not found on PATH; proceeding without compiler cache configuration."
fi


# Debug
if [[ "$DEBUG" == "true" ]]; then
  echo "ac_add_options --enable-debug" >> mozconfig

  # On Mac, disable the content sandbox for opening Flat Omni.ja in debug builds
  if [[ "$PLATFORM" == "mac" ]]; then
    echo "mk_add_options 'export MOZ_DISABLE_CONTENT_SANDBOX=1'" >> mozconfig
  fi
fi

# PGO
if [[ "$PGO" == "true" ]]; then
  if [[ "$PGO_MODE" == "generate" ]]; then
    # Use profile-generate for cross-platform builds
    echo 'ac_add_options --enable-profile-generate=cross' >> mozconfig
  elif [[ "$PGO_MODE" == "use" && -n "$PGO_ARTIFACT_NAME" ]]; then
    # Use a downloaded profile by its artifact name
    echo 'export MOZ_LTO=cross' >> mozconfig
    echo 'ac_add_options --enable-profile-use=cross' >> mozconfig
    echo 'ac_add_options --with-pgo-profile-path=$(echo ~)/artifacts/merged.profdata' >> mozconfig
    echo 'ac_add_options --with-pgo-jarlog=$(echo ~)/artifacts/en-US.log' >> mozconfig
  fi
fi

# Canonical Debug enables tests; legacy Debug+PGO remains noncanonical.
python3 .github/workflows/scripts/qa3_build_profile.py \
  --config mozconfig "${PROFILE_ARGS[@]}"

sudo apt update -y
sudo apt install msitools -y

# Update Channel

# Replace long MOZ_APPUPDATE URL with NORA update host | Floorp
OLD='https://@MOZ_APPUPDATE_HOST@/update/6/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%SYSTEM_CAPABILITIES%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml'
NEW='https://%NORA_UPDATE_HOST%update.xml'
sed -i "s|${OLD}|${NEW}|g" ./build/application.ini.in

# Note: Rust toolchain and targets are configured in setup-rust.sh
# Ensure targets are available after any toolchain changes
if [[ "$PGO" == "true" && "$PGO_MODE" == "use" ]]; then
  echo "PGO use mode - re-adding Rust targets if needed..."
  
  if [[ "$PLATFORM" == "mac" ]]; then
    if [[ "$ARCH" == "x86_64" ]]; then
      TARGET="x86_64-apple-darwin"
    else
      TARGET="aarch64-apple-darwin"
    fi
    rustup target add "$TARGET" 2>/dev/null || true
  elif [[ "$PLATFORM" == "linux" ]]; then
    if [[ "$ARCH" == "aarch64" ]]; then
      rustup target add aarch64-unknown-linux-gnu 2>/dev/null || true
    else
      rustup target add x86_64-unknown-linux-gnu 2>/dev/null || true
    fi
  elif [[ "$PLATFORM" == "windows" ]]; then
    if [[ "$ARCH" == "aarch64" ]]; then
      rustup target add aarch64-pc-windows-msvc 2>/dev/null || true
    else
      rustup target add x86_64-pc-windows-msvc 2>/dev/null || true
    fi
  fi
fi

./mach --no-interactive bootstrap --application-choice browser
