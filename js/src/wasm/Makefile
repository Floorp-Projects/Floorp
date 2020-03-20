.PHONY: help update run expectations

help:
	@echo "Script to regenerate wasm test cases (JS and WPT) using the wasm-generate-testsuite tool."
	@echo ""
	@echo "- 'make update' runs wasm-generate-testsuite and puts the results in"
	@echo "  the right directories, then updates the WPT manifest, if needed."
	@echo ""
	@echo "  The wasm-generate-testsuite repo is needed under js/src/wasm (not"
	@echo "  checked in). It can be a symbolic link or a real directory; if it's"
	@echo "  not there, the Makefile will clone the repo from the sources."
	@echo ""
	@echo "  Generation of a testsuite is driven by js/src/jit-test/etc/wasm-config.toml"
	@echo "  If you need to exclude a test, change test directives, or add a"
	@echo "  proposal; that is the file to modify."
	@echo ""
	@echo "  Generated testsuites are made reproducible using 'wasm-config-lock.toml'"
	@echo "  which contains pinned commits from the last build. New testsuites will"
	@echo "  remain on those commits by default. To update to the latest version for"
	@echo "  a repo, delete it from the lock file and do a build."
	@echo ""
	@echo "- 'MOZCONFIG=/path/to/bin/firefox make run' runs the WPT test cases and prints a"
	@echo "  summary of the failures in the console."
	@echo ""
	@echo "- 'MOZCONFIG=/path/to/bin/firefox make expectations' runs the WPT test cases and"
	@echo "  updates the expectations (known failures)."
	@echo ""
	@echo "Choose a rule: update, run, or expectations."

update:
	[ -d ./wasm-generate-testsuite ] || git clone https://github.com/eqrion/wasm-generate-testsuite ./wasm-generate-testsuite
	cp ../jit-test/etc/wasm-config.toml ./wasm-generate-testsuite/config.toml
	cp ../jit-test/etc/wasm-config-lock.toml ./wasm-generate-testsuite/config-lock.toml
	(cd ./wasm-generate-testsuite && cargo run)
	cp ./wasm-generate-testsuite/config-lock.toml ../jit-test/etc/wasm-config-lock.toml
	rm -r ../jit-test/tests/wasm/spec
	cp -R wasm-generate-testsuite/tests/js ../jit-test/tests/wasm/spec
	[ ! -d ../jit-test/etc/wasm-spec-tests.patch ] || (cd ../jit-test/tests/wasm/spec && patch -u -p7 < ../../../etc/wasm-spec-tests.patch)
	rm -r ../../../testing/web-platform/mozilla/tests/wasm
	cp -R wasm-generate-testsuite/tests/wpt ../../../testing/web-platform/mozilla/tests/wasm

run:
	@[ -z $(MOZCONFIG) ] && echo "You need to define the MOZCONFIG env variable first."
	@[ -z $(MOZCONFIG) ] || ../../../mach wpt /_mozilla/wasm

expectations:
	@[ -z $(MOZCONFIG) ] && echo "You need to define the MOZCONFIG env variable first." || true
	@[ -z $(MOZCONFIG) ] || ../../../mach wpt /_mozilla/wasm --log-raw /tmp/expectations.log || true
	@[ -z $(MOZCONFIG) ] || ../../../mach wpt-update /tmp/expectations.log --no-patch
