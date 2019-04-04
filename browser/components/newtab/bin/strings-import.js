#! /usr/bin/env node
"use strict";

/* eslint-disable no-console */
const fetch = require("node-fetch");

/* globals cd, ls, mkdir, rm, ShellString */
require("shelljs/global");

const {CENTRAL_LOCALES, DEFAULT_LOCALE, LOCALES_SOURCE_DIRECTORY} = require("./locales");
const L10N_CENTRAL = "https://hg.mozilla.org/l10n-central";
const PROPERTIES_PATH = "raw-file/default/browser/chrome/browser/activity-stream/newtab.properties";
const STRINGS_FILE = "strings.properties";

// Get all the locales in l10n-central
async function getLocales() {
  console.log(`Getting locales from ${L10N_CENTRAL}`);

  // Add sub repository locales that mozilla-central builds
  const locales = [];
  const unbuilt = [];
  const subrepos = await (await fetch(`${L10N_CENTRAL}?style=json`)).json();
  subrepos.entries.forEach(({name}) => {
    if (CENTRAL_LOCALES.includes(name)) {
      locales.push(name);
    } else {
      unbuilt.push(name);
    }
  });

  console.log(`Got ${locales.length} mozilla-central locales: ${locales}`);
  console.log(`Skipped ${unbuilt.length} unbuilt locales: ${unbuilt}`);

  return locales;
}

// Pick a different string if the desired one is missing
const transvision = {};
async function cherryPickString(locale) {
  const getTransvision = async string => {
    if (!transvision[string]) {
      const response = await fetch(`https://transvision.mozfr.org/api/v1/entity/gecko_strings/?id=${string}`); // eslint-disable-line fetch-options/no-fetch-credentials
      transvision[string] = response.ok ? await response.json() : {};
    }
    return transvision[string];
  };
  const expectedKey = "section_menu_action_add_search_engine";
  const expected = await getTransvision(`browser/chrome/browser/activity-stream/newtab.properties:${expectedKey}`);
  const target = await getTransvision("browser/chrome/browser/search.properties:searchAddFoundEngine2");
  return !expected[locale] && target[locale] ? `${expectedKey}=${target[locale]}\n` : "";
}

// Save the properties file to the locale's directory
async function saveProperties(locale) {
  // Only save a file if the repository has the file
  const url = `${L10N_CENTRAL}/${locale}/${PROPERTIES_PATH}`;
  const response = await fetch(url); // eslint-disable-line fetch-options/no-fetch-credentials
  if (!response.ok) {
    // Indicate that this locale didn't save
    return locale;
  }

  // Save the file to the right place
  const text = await response.text();
  mkdir(locale);
  cd(locale);
  // For now, detect if a string is missing to use a different one instead
  ShellString(text + await cherryPickString(locale)).to(STRINGS_FILE);
  cd("..");

  // Indicate that we were successful in saving
  return "";
}

// Replace and update each locale's strings
async function updateLocales() {
  console.log(`Switching to and deleting existing l10n tree under: ${LOCALES_SOURCE_DIRECTORY}`);

  cd(LOCALES_SOURCE_DIRECTORY);
  ls().forEach(dir => {
    // Keep the default/source locale as it might have newer strings
    if (dir !== DEFAULT_LOCALE) {
      rm("-r", dir);
    }
  });

  // Save the properties file for each locale one at a time to avoid too many
  // parallel connections (resulting in ECONNRESET / socket hang up)
  const missing = [];
  for (const locale of await getLocales()) {
    process.stdout.write(`${locale} `);
    if (await saveProperties(locale)) {
      missing.push(locale);
    }
  }

  console.log("");
  console.log(`Skipped ${missing.length} locales without strings: ${missing.sort()}`);

  console.log(`
Please check the diffs, add/remove files, and then commit the result. Suggested commit message:
chore(l10n): Update from l10n-central ${new Date()}`);
}

updateLocales().catch(console.error);
