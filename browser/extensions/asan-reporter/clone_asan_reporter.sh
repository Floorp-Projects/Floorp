#!/bin/sh

mkdir tmp/
git clone --no-checkout --depth 1 https://github.com/choller/firefox-asan-reporter tmp/
(cd tmp && git reset --hard d508c6e3f5df752a9a7a2d6f1e4e7261ec2290e7)

# Copy only whitelisted files
cp tmp/bootstrap.js tmp/install.rdf.in tmp/moz.build tmp/README.md tmp/LICENSE .

# Remove the temporary directory
rm -Rf tmp/
