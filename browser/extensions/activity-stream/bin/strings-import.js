#! /usr/bin/env node
"use strict";

/* eslint-disable no-console */
const fetch = require("node-fetch");

/* globals cd, ls, mkdir, rm, ShellString */
require("shelljs/global");

const DEFAULT_LOCALE = "en-US";
const L10N_CENTRAL = "https://hg.mozilla.org/l10n-central";
const PROPERTIES_PATH = "raw-file/default/browser/chrome/browser/activity-stream/newtab.properties";
const STRINGS_FILE = "strings.properties";

// Get all the locales in l10n-central
async function getLocales() {
  console.log(`Getting locales from ${L10N_CENTRAL}`);

  // Add all non-test sub repository locales
  const locales = [];
  const subrepos = await (await fetch(`${L10N_CENTRAL}?style=json`)).json();
  subrepos.entries.forEach(({name}) => {
    if (name !== "x-testing") {
      locales.push(name);
    }
  });

  console.log(`Got ${locales.length} locales: ${locales}`);
  return locales;
}

// Save the properties file to the locale's directory
async function saveProperties(locale) {
  // Only save a file if the repository has the file
  const url = `${L10N_CENTRAL}/${locale}/${PROPERTIES_PATH}`;
  const response = await fetch(url);
  if (!response.ok) {
    // Indicate that this locale didn't save
    return locale;
  }

  // Save the file to the right place
  const text = await response.text();
  mkdir(locale);
  cd(locale);
  ShellString(text).to(STRINGS_FILE);
  cd("..");

  // Indicate that we were successful in saving
  return "";
}

// Replace and update each locale's strings
async function updateLocales() {
  console.log("Switching to and deleting existing l10n tree under: locales");

  cd("locales");
  ls().forEach(dir => {
    // Keep the default/source locale as it might have newer strings
    if (dir !== DEFAULT_LOCALE) {
      rm("-r", dir);
    }
  });

  // Save the properties file for each locale in parallel
  const locales = await getLocales();
  const missing = (await Promise.all(locales.map(saveProperties))).filter(v => v);
  console.log(`Skipped ${missing.length} locales without strings: ${missing.sort()}`);

  console.log(`
Please check the diffs, add/remove files, and then commit the result. Suggested commit message:
chore(l10n): Update from l10n-central ${new Date()}`);
}

updateLocales().catch(console.error);
