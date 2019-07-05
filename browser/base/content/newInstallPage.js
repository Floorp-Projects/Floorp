/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global RPMGetUpdateChannel, RPMGetFxAccountsEndpoint */

const PARAMS = new URL(location).searchParams;
const ENTRYPOINT = "new-install-page";
const SOURCE = `new-install-page-${RPMGetUpdateChannel()}`;
const CAMPAIGN = "dedicated-profiles";
const ENDPOINT = PARAMS.get("endpoint");

function appendAccountsParams(url) {
  url.searchParams.set("entrypoint", ENTRYPOINT);
  url.searchParams.set("utm_source", SOURCE);
  url.searchParams.set("utm_campaign", CAMPAIGN);
}

function appendParams(url, params) {
  appendAccountsParams(url);

  for (let [key, value] of Object.entries(params)) {
    url.searchParams.set(key, value);
  }
}

async function requestFlowMetrics() {
  let requestURL = new URL(await endpoint);
  requestURL.pathname = "metrics-flow";
  appendParams(requestURL, {
    form_type: "email",
  });

  let response = await fetch(requestURL, { credentials: "omit" });
  if (response.status === 200) {
    return response.json();
  }

  throw new Error(`Failed to retrieve metrics: ${response.status}`);
}

async function submitForm(event) {
  // We never want to submit the form.
  event.preventDefault();

  let input = document.getElementById("sync-input");

  let { flowId, flowBeginTime } = await metrics;

  let requestURL = new URL(await endpoint);
  appendParams(requestURL, {
    action: "email",
    utm_campaign: CAMPAIGN,
    email: input.value,
    flow_id: flowId,
    flow_begin_time: flowBeginTime,
  });

  window.open(requestURL, "_blank", "noopener");
  document.getElementById("sync").hidden = true;
}

const endpoint = RPMGetFxAccountsEndpoint(ENTRYPOINT);

// This must come before the CSP is set or it will be blocked.
const metrics = requestFlowMetrics();

document.addEventListener(
  "DOMContentLoaded",
  () => {
    document.getElementById("sync").addEventListener("submit", submitForm);
  },
  { once: true }
);
