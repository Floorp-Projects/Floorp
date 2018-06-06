#! /usr/bin/env node
"use strict";

const MIN_FIREFOX_VERSION = "55.0a1";

/* globals cd, mv, sed */
require("shelljs/global");

cd(process.argv[2]);

// Convert install.rdf.in to install.rdf without substitutions
mv("install.rdf.in", "install.rdf");
sed("-i", /^#filter substitution/, "", "install.rdf");
sed("-i", /(<em:minVersion>).+(<\/em:minVersion>)/, `$1${MIN_FIREFOX_VERSION}$2`, "install.rdf");
sed("-i", /(<em:maxVersion>).+(<\/em:maxVersion>)/, "$1*$2", "install.rdf");

// Convert jar.mn to chrome.manifest with just manifest
mv("jar.mn", "chrome.manifest");
sed("-i", /^[^%].*$/, "", "chrome.manifest");
sed("-i", /^% (content.*) %(.*)$/, "$1 $2", "chrome.manifest");
sed("-i", /^% (resource.*) %.*$/, "$1 .", "chrome.manifest");
