#!/usr/bin/env bash
# Re-sign the existing authenticated index; never upload or remove a DEB.
set -euo pipefail

for ppa_required in KEYMASK GPG_SEC GPG_SSB PPA_FTP_URL PPA_FTP_USER PPA_FTP_PASS PPA_CANDIDATE PPA_CANDIDATE_SHA256; do
  if [ -z "${!ppa_required:-}" ]; then
    echo "[ppa-hold] Required setting is missing: $ppa_required" >&2
    exit 1
  fi
done
echo "::add-mask::$KEYMASK"
PPA_SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PPA_POLICY="$PPA_SCRIPT_DIR/../ppa-withheld-versions.json"
PPA_WORKDIR=$(mktemp -d)
trap 'rm -rf "$PPA_WORKDIR"' EXIT
mkdir -p "$PPA_WORKDIR/before" "$PPA_WORKDIR/publish" "$PPA_WORKDIR/gnupg"
chmod 700 "$PPA_WORKDIR/gnupg"
export GNUPGHOME="$PPA_WORKDIR/gnupg"

cd "$PPA_WORKDIR/before"
lftp -u "$PPA_FTP_USER","$PPA_FTP_PASS" "$PPA_FTP_URL" <<'LFTPEOF'
  set ftp:passive-mode true
  set ftp:prefer-epsv false
  set net:timeout 30
  set net:max-retries 3
  set cmd:fail-exit true
  cd amd64
  mget Packages Packages.gz Release Release.gpg InRelease
  quit
LFTPEOF
lftp -u "$PPA_FTP_USER","$PPA_FTP_PASS" "$PPA_FTP_URL" > "$PPA_WORKDIR/before-debs.txt" <<'LFTPEOF'
  set ftp:passive-mode true
  set ftp:prefer-epsv false
  set net:timeout 30
  set net:max-retries 3
  set cmd:fail-exit true
  cd amd64
  cls -1B floorp-*.deb
  quit
LFTPEOF
grep -Fx "floorp-$PPA_CANDIDATE.deb" "$PPA_WORKDIR/before-debs.txt" >/dev/null

printf '%s\n' "$GPG_SEC" > "$PPA_WORKDIR/signing-key.asc"
gpg --batch --import "$PPA_WORKDIR/signing-key.asc"
rm "$PPA_WORKDIR/signing-key.asc"
gpg --batch --export "$GPG_SSB" > "$PPA_WORKDIR/trusted-keyring.gpg"
test -s "$PPA_WORKDIR/trusted-keyring.gpg"
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" Release.gpg Release
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" --output "$PPA_WORKDIR/authenticated-release" InRelease
cmp Release "$PPA_WORKDIR/authenticated-release"
python3 "$PPA_SCRIPT_DIR/ppa_index.py" verify-release .
cp Packages "$PPA_WORKDIR/publish/Packages"

cd "$PPA_WORKDIR/publish"
python3 "$PPA_SCRIPT_DIR/ppa_index.py" filter "$PPA_POLICY" Packages \
  --expected-version "$PPA_CANDIDATE" --expected-sha256 "$PPA_CANDIDATE_SHA256" \
  --report "$PPA_WORKDIR/hold-result.json"
PPA_CANDIDATE_VERSION=$(python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["expectedCandidate"])' "$PPA_WORKDIR/hold-result.json")
curl -sSfL --connect-timeout 15 --max-time 180 \
  -o "$PPA_WORKDIR/candidate.deb" "https://ppa.floorp.app/amd64/floorp-$PPA_CANDIDATE.deb"
PPA_DOWNLOADED_SHA256=$(sha256sum "$PPA_WORKDIR/candidate.deb" | cut -d' ' -f1)
test "$PPA_DOWNLOADED_SHA256" = "$PPA_CANDIDATE_SHA256"
test "$(dpkg-deb -f "$PPA_WORKDIR/candidate.deb" Version)" = "$PPA_CANDIDATE_VERSION"
echo "[ppa-hold] Candidate DEB version $PPA_CANDIDATE_VERSION verified before publication; SHA256 $PPA_DOWNLOADED_SHA256."
python3 "$PPA_SCRIPT_DIR/test_ppa_index.py"
gzip -n -k -f Packages
apt-ftparchive release . > Release
gpg --batch --default-key "$GPG_SSB" -abs -o Release.gpg Release
gpg --batch --default-key "$GPG_SSB" --clearsign -o InRelease Release
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" Release.gpg Release
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" --output "$PPA_WORKDIR/new-authenticated-release" InRelease
cmp Release "$PPA_WORKDIR/new-authenticated-release"
python3 "$PPA_SCRIPT_DIR/ppa_index.py" verify-release .
python3 "$PPA_SCRIPT_DIR/ppa_index.py" apt-candidates Packages "$PPA_CANDIDATE" 12.20.0
python3 "$PPA_SCRIPT_DIR/ppa_index.py" apt-candidates Packages "$PPA_CANDIDATE" 12.20.0~build1

# Refuse a race with another publisher after validating the old signature.
lftp -u "$PPA_FTP_USER","$PPA_FTP_PASS" "$PPA_FTP_URL" <<LFTPEOF
  set ftp:passive-mode true
  set ftp:prefer-epsv false
  set net:timeout 30
  set net:max-retries 3
  set cmd:fail-exit true
  cd amd64
  get InRelease -o $PPA_WORKDIR/current-InRelease
  quit
LFTPEOF
cmp "$PPA_WORKDIR/before/InRelease" "$PPA_WORKDIR/current-InRelease"

lftp -u "$PPA_FTP_USER","$PPA_FTP_PASS" "$PPA_FTP_URL" <<'LFTPEOF'
  set ftp:passive-mode true
  set ftp:prefer-epsv false
  set net:timeout 30
  set net:max-retries 3
  set cmd:fail-exit true
  cd amd64
  put Packages -o tmp.Packages
  mv tmp.Packages Packages
  put Packages.gz -o tmp.Packages.gz
  mv tmp.Packages.gz Packages.gz
  put Release -o tmp.Release
  mv tmp.Release Release
  put Release.gpg -o tmp.Release.gpg
  mv tmp.Release.gpg Release.gpg
  put InRelease -o tmp.InRelease
  mv tmp.InRelease InRelease
  quit
LFTPEOF

lftp -u "$PPA_FTP_USER","$PPA_FTP_PASS" "$PPA_FTP_URL" > "$PPA_WORKDIR/after-debs.txt" <<'LFTPEOF'
  set ftp:passive-mode true
  set ftp:prefer-epsv false
  set net:timeout 30
  set net:max-retries 3
  set cmd:fail-exit true
  cd amd64
  cls -1B floorp-*.deb
  quit
LFTPEOF
sort "$PPA_WORKDIR/before-debs.txt" > "$PPA_WORKDIR/before-debs-sorted.txt"
sort "$PPA_WORKDIR/after-debs.txt" > "$PPA_WORKDIR/after-debs-sorted.txt"
diff -u "$PPA_WORKDIR/before-debs-sorted.txt" "$PPA_WORKDIR/after-debs-sorted.txt"

# Use the normal public URLs and require the whole signed set to agree.
mkdir "$PPA_WORKDIR/public"
ppa_public_ok=0
for ppa_attempt in $(seq 1 120); do
  ppa_matches=1
  for ppa_metadata in Packages Packages.gz Release Release.gpg InRelease; do
    if ! curl -sSfL --connect-timeout 15 --max-time 30 \
      -o "$PPA_WORKDIR/public/$ppa_metadata" "https://ppa.floorp.app/amd64/$ppa_metadata"; then
      ppa_matches=0
      break
    fi
    if ! cmp -s "$ppa_metadata" "$PPA_WORKDIR/public/$ppa_metadata"; then
      ppa_matches=0
      break
    fi
  done
  if [ "$ppa_matches" -eq 1 ]; then
    ppa_public_ok=1
    break
  fi
  echo "[ppa-hold] Public signed index has not propagated (attempt $ppa_attempt)."
  sleep 5
done
if [ "$ppa_public_ok" -ne 1 ]; then
  echo "[ppa-hold] Public signed index could not be verified." >&2
  exit 1
fi
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" "$PPA_WORKDIR/public/Release.gpg" "$PPA_WORKDIR/public/Release"
gpgv --keyring "$PPA_WORKDIR/trusted-keyring.gpg" --output "$PPA_WORKDIR/public/authenticated-release" "$PPA_WORKDIR/public/InRelease"
cmp "$PPA_WORKDIR/public/Release" "$PPA_WORKDIR/public/authenticated-release"
python3 "$PPA_SCRIPT_DIR/ppa_index.py" verify-release "$PPA_WORKDIR/public"
python3 "$PPA_SCRIPT_DIR/ppa_index.py" apt-candidates "$PPA_WORKDIR/public/Packages" "$PPA_CANDIDATE" 12.20.0
python3 "$PPA_SCRIPT_DIR/ppa_index.py" apt-candidates "$PPA_WORKDIR/public/Packages" "$PPA_CANDIDATE" 12.20.0~build1
curl -sSfL --connect-timeout 15 --max-time 180 \
  -o "$PPA_WORKDIR/candidate.deb" "https://ppa.floorp.app/amd64/floorp-$PPA_CANDIDATE.deb"
PPA_DOWNLOADED_SHA256=$(sha256sum "$PPA_WORKDIR/candidate.deb" | cut -d' ' -f1)
test "$PPA_DOWNLOADED_SHA256" = "$PPA_CANDIDATE_SHA256"
test "$(dpkg-deb -f "$PPA_WORKDIR/candidate.deb" Version)" = "$PPA_CANDIDATE_VERSION"
echo "[ppa-hold] Public DEB version $PPA_CANDIDATE_VERSION verified; SHA256 $PPA_DOWNLOADED_SHA256."
sha256sum Packages Packages.gz Release Release.gpg InRelease
echo "[ppa-hold] Public signed index verified; candidate $PPA_CANDIDATE_VERSION; DEB archive unchanged."
PPA_QA_DIR="${RUNNER_TEMP:?}/floorp-ppa-hold-public-evidence"
mkdir -p "$PPA_QA_DIR/before" "$PPA_QA_DIR/after"
for ppa_metadata in Packages Packages.gz Release Release.gpg InRelease; do
  cp "$PPA_WORKDIR/before/$ppa_metadata" "$PPA_QA_DIR/before/$ppa_metadata"
  cp "$PPA_WORKDIR/public/$ppa_metadata" "$PPA_QA_DIR/after/$ppa_metadata"
done
cp "$PPA_WORKDIR/hold-result.json" "$PPA_QA_DIR/hold-result.json"
cp "$PPA_WORKDIR/before-debs-sorted.txt" "$PPA_QA_DIR/before-debs.txt"
cp "$PPA_WORKDIR/after-debs-sorted.txt" "$PPA_QA_DIR/after-debs.txt"
printf 'product_version=%s\nversion=%s\nsha256=%s\n' "$PPA_CANDIDATE" "$PPA_CANDIDATE_VERSION" "$PPA_DOWNLOADED_SHA256" > "$PPA_QA_DIR/public-deb.txt"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  printf 'PPA candidate: %s. Public signed index verified. DEB archive unchanged.\n' "$PPA_CANDIDATE_VERSION" >> "$GITHUB_STEP_SUMMARY"
  cat "$PPA_WORKDIR/hold-result.json" >> "$GITHUB_STEP_SUMMARY"
fi
