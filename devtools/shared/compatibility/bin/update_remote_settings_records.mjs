/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global process */

// This should be called with 2 env variable: AUTH and SERVER (see VALID_SERVERS)
// e.g. `SERVER=stage AUTH='Bearer XXXXXXXXXXXXX' yarn update-rs-records`

// The Compatibility panel detects issues by comparing against official MDN compatibility data
// at https://github.com/mdn/browser-compat-data.

// The subsets from the dataset required by the Compatibility panel are:
// * browsers: https://github.com/mdn/browser-compat-data/tree/main/browsers
// * css.properties: https://github.com/mdn/browser-compat-data/tree/main/css

// The MDN compatibility data is available as a node package ("@mdn/browser-compat-data").
// This node script fetches `browsers.json` and `css-properties.json` and updates records
// from the appropriate collection in RemoteSettings.

import fetch from "node-fetch";

// Use the legacy wrapper to support all Node 12+ versions.
// If we only support Node 16+, can be updated to:
//   import bcd from '@mdn/browser-compat-data' assert { type: 'json' };
// See https://github.com/mdn/browser-compat-data.
import compatData from "@mdn/browser-compat-data/forLegacyNode";

if (!process.env.AUTH) {
  throw new Error(`AUTH environment variable needs to be set`);
}

const VALID_SERVERS = ["dev", "stage", "prod"];
if (!VALID_SERVERS.includes(process.env.SERVER)) {
  throw new Error(
    `SERVER environment variable needs to be set to one of the following values: ${VALID_SERVERS.join(
      ", "
    )}`
  );
}

const rsUrl = `https://settings-writer.${process.env.SERVER}.mozaws.net`;
const rsBrowsersCollectionEndpoint = `${rsUrl}/v1/buckets/main-workspace/collections/devtools-compatibility-browsers`;
const rsBrowsersRecordsEndpoint = `${rsBrowsersCollectionEndpoint}/records`;

(async function() {
  console.log(`Get existing records from ${rsUrl}`);
  const records = await getRSRecords();

  const operations = { added: [], updated: [], removed: [] };

  const browsersMdn = getFlatBrowsersMdnData();
  for (const browserMdn of browsersMdn) {
    const rsRecord = records.find(
      record =>
        record.browserid == browserMdn.browserid &&
        record.version == browserMdn.version
    );
    if (browserMdn.status == "retired") {
      if (rsRecord) {
        const succesful = await deleteRecord(rsRecord);
        if (succesful) {
          operations.removed.push(rsRecord);
        }
      }
      continue;
    }

    if (!rsRecord) {
      const succesful = await createRecord(browserMdn);
      if (succesful) {
        operations.added.push(browserMdn);
      }
      continue;
    }

    if (
      rsRecord.status !== browserMdn.status ||
      rsRecord.name !== browserMdn.name
    ) {
      const succesful = await updateRecord(rsRecord, browserMdn);
      if (succesful) {
        operations.updated.push(browserMdn);
      }
    }
  }

  for (const record of records) {
    const browserMdn = browsersMdn.find(
      browser =>
        browser.browserid == record.browserid &&
        browser.version == record.version
    );
    if (!browserMdn) {
      const succesful = await deleteRecord(record);
      if (succesful) {
        operations.removed.push(record);
      }
    }
  }

  console.group("Results");
  console.log("Added:", operations.added.length);
  if (operations.added.length > 0) {
    console.table(operations.added);
  }
  console.log("Updated:", operations.updated.length);
  if (operations.updated.length > 0) {
    console.table(operations.updated);
  }
  console.log("Removed:", operations.removed.length);
  if (operations.removed.length > 0) {
    console.table(operations.removed);
  }
  console.groupEnd();

  if (
    operations.added.length +
      operations.updated.length +
      operations.removed.length ==
    0
  ) {
    console.log("No changes detected");
  } else {
    const refreshedRecords = await getRSRecords();
    console.log("Browsers data synced ✅\nRefreshed records:");
    console.table(refreshedRecords);
    console.log("Requesting review");
    await requestReview();
    console.log("Review requested ✅");
  }
})();

async function getRSRecords() {
  const response = await fetch(rsBrowsersRecordsEndpoint, {
    method: "GET",
    headers: {
      "Content-Type": "application/json",
      Authorization: process.env.AUTH,
    },
  });
  if (response.status !== 200) {
    throw new Error(
      `Can't retrieve records: "[${response.status}] ${response.statusText}"`
    );
  }
  const { data } = await response.json();
  return data;
}

/**
 * Create a record on RemoteSetting
 *
 * @param {Object} browserMdn: An item from the result of getFlatBrowsersMdnData
 * @returns {Boolean} Whether the API call was succesful or not
 */
async function createRecord(browserMdn) {
  console.log("Create", browserMdn.browserid, browserMdn.version);
  const response = await fetch(`${rsBrowsersRecordsEndpoint}`, {
    method: "POST",
    body: JSON.stringify({ data: browserMdn }),
    headers: {
      "Content-Type": "application/json",
      Authorization: process.env.AUTH,
    },
  });
  const succesful = response.status == 201;
  if (!succesful) {
    console.warn(
      `Couldn't create record: "[${response.status}] ${response.statusText}"`
    );
  }
  return succesful;
}

/**
 * Update a record on RemoteSetting
 *
 * @param {Object} record: The existing record on RemoteSetting
 * @param {Object} browserMdn: An item from the result of getFlatBrowsersMdnData whose data
 *                             will be put into the record.
 * @returns {Boolean} Whether the API call was succesful or not
 */
async function updateRecord(record, browserMdn) {
  console.log("Update", record.browserid, record.version);
  const response = await fetch(`${rsBrowsersRecordsEndpoint}/${record.id}`, {
    method: "PUT",
    body: JSON.stringify({ data: browserMdn }),
    headers: {
      "Content-Type": "application/json",
      Authorization: process.env.AUTH,
    },
  });
  const succesful = response.status == 200;
  if (!succesful) {
    console.warn(
      `Couldn't update record: "[${response.status}] ${response.statusText}"`
    );
  }
  return succesful;
}

/**
 * Remove a record on RemoteSetting
 *
 * @param {Object} record: The existing record on RemoteSetting
 * @returns {Boolean} Whether the API call was succesful or not
 */
async function deleteRecord(record) {
  console.log("Delete", record.browserid, record.version);
  const response = await fetch(`${rsBrowsersRecordsEndpoint}/${record.id}`, {
    method: "DELETE",
    headers: {
      "Content-Type": "application/json",
      Authorization: process.env.AUTH,
    },
  });
  const succesful = response.status == 200;
  if (!succesful) {
    console.warn(
      `Couldn't delete record: "[${response.status}] ${response.statusText}"`
    );
  }
  return succesful;
}

/**
 * Ask for review on the collection.
 */
async function requestReview() {
  const response = await fetch(rsBrowsersCollectionEndpoint, {
    method: "PATCH",
    body: JSON.stringify({ data: { status: "to-review" } }),
    headers: {
      "Content-Type": "application/json",
      Authorization: process.env.AUTH,
    },
  });
  if (response.status !== 200) {
    console.warn(
      `Couldn't request review: "[${response.status}] ${response.statusText}"`
    );
  }
}

function getFlatBrowsersMdnData() {
  const browsers = [];
  for (const [browserid, browserInfo] of Object.entries(compatData.browsers)) {
    for (const [releaseNumber, releaseInfo] of Object.entries(
      browserInfo.releases
    )) {
      if (!browserInfo.name) {
        console.error(
          `${browserid} "name" property is expected but wasn't found`,
          browserInfo
        );
        continue;
      }

      if (!releaseInfo.status) {
        console.error(
          `${browserid} "status" property is expected but wasn't found`,
          releaseInfo
        );
        continue;
      }

      if (!releaseNumber || !releaseNumber.match(/\d/)) {
        console.error(
          `${browserid} "releaseNumber" doesn't have expected shape`,
          releaseNumber
        );
        continue;
      }

      browsers.push({
        browserid,
        name: browserInfo.name,
        status: releaseInfo.status,
        version: releaseNumber,
      });
    }
  }
  return browsers;
}
