#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://github.com/choller/firefox-asan-reporter tmp/
(cd tmp && git reset --hard ac5dc3f95b41429be79a7dcf8bf13a1075e850be)

# Copy only whitelisted files
cp tmp/bootstrap.js tmp/install.rdf.in tmp/moz.build tmp/README.md tmp/LICENSE .

# Remove the temporary directory
rm -Rf tmp/
