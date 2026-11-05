#!/bin/bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
run_dir=$(mktemp -d /workspace/qa-linux-additional/KAN-24/api-validation-XXXXXX)
cp -- "$script_dir/run.py" "$script_dir/cases.js" "$script_dir/canonical.js" "$script_dir/proposed.js" "$script_dir/validate-result.py" "$run_dir/"
dbus-run-session -- python3 "$run_dir/run.py" --product /workspace/qa-linux-full-build/products/3040-r2/floorp
python3 "$run_dir/validate-result.py" "$run_dir/result.json"
printf 'Evidence: %s\n' "$run_dir"
