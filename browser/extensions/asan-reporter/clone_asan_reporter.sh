#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://github.com/choller/firefox-asan-reporter tmp/
(cd tmp && git reset --hard e1a5d7dc5a2706af72bac0f11eab34b3afdb48ba)

# Copy only whitelisted files
cp tmp/bootstrap.js tmp/install.rdf.in tmp/moz.build tmp/README.md tmp/LICENSE .

# Remove the temporary directory
rm -Rf tmp/
