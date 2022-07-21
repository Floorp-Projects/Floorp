/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { PanelTestProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/PanelTestProvider.jsm"
);

const CWD = Services.dirsvc.get("CurWorkD", Ci.nsIFile).path;
const CORPUS_DIR = PathUtils.join(CWD, "corpus");

const CORPUS = [
  {
    name: "CFRMessageProvider.messages.json",
    provider: CFRMessageProvider,
  },
  {
    name: "OnboardingMessageProvider.messages.json",
    provider: OnboardingMessageProvider,
  },
  {
    name: "PanelTestProvider.messages.json",
    provider: PanelTestProvider,
  },
  {
    name: "PanelTestProvider_toast_notification.messages.json",
    provider: PanelTestProvider,
    filter: message => message.template === "toast_notification",
  },
];

let exit = false;
async function main() {
  try {
    await IOUtils.makeDirectory(CORPUS_DIR);

    for (const entry of CORPUS) {
      const { name, provider } = entry;
      const filter = entry.filter ?? (() => true);
      const messages = await provider.getMessages();
      const json = `${JSON.stringify(messages.filter(filter), undefined, 2)}\n`;

      const path = PathUtils.join(CORPUS_DIR, name);
      await IOUtils.writeUTF8(path, json);
    }
  } finally {
    exit = true;
  }
}

main();

// We need to spin the event loop here, otherwise everything goes out of scope.
Services.tm.spinEventLoopUntil(
  "extract-test-corpus.js: waiting for completion",
  () => exit
);
