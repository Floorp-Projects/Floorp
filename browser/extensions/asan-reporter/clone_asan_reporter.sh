#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://github.com/choller/firefox-asan-reporter tmp/
(cd tmp && git reset --hard c42a0b9c131c90cec2a2e93efb77e02e1673316f)

# Copy only whitelisted files
cp tmp/bootstrap.js tmp/install.rdf.in tmp/moz.build tmp/README.md tmp/LICENSE .

# Remove the temporary directory
rm -Rf tmp/
