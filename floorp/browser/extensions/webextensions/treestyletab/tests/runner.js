/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  nextFrame,
  configs
} from '/common/common.js';

import * as ApiTabsListener from '/background/api-tabs-listener.js';
import { Diff } from '/common/diff.js';
import * as Utils from './utils.js';

import * as TestDOMUpdater from './test-dom-updater.js';
import * as TestContextMenu from './test-context-menu.js';
import * as TestHidden from './test-hidden.js';
import * as TestNewTab from './test-new-tab.js';
import * as TestSuccessor from './test-successor.js';
import * as TestTree from './test-tree.js';
import * as TestCloseParentBehavior from './test-close-parent-behavior.js';


const NO_RESET_CONFIG_KEYS = new Set([
  'debug',
  'logFor',
  'style', // prevent reloading of the background page
  'useCachedTree' // prevent reloading of the background page
]);

let mResults;
let mLogs;

async function run() {
  await configs.$loaded;
  ApiTabsListener.init();
  ApiTabsListener.start();
  mResults = document.getElementById('results');
  mLogs = document.getElementById('logs');
  const configValues = backupConfigs();
  await runAll();
  ApiTabsListener.destroy();
  await restoreConfigs(configValues);
}

function backupConfigs() {
  const values = {};
  for (const key of Object.keys(configs.$default).sort()) {
    values[key] = configs[key];
  }
  return JSON.parse(JSON.stringify(values));
}

async function restoreConfigs(values) {
  const resetConfigs = {};
  for (const key of Object.keys(values)) {
    if (NO_RESET_CONFIG_KEYS.has(key))
      continue;
    resetConfigs[key] = values[key];
  }
  await Utils.setConfigs(resetConfigs);
  // wait until updated configs are delivered to other namespaces
  await nextFrame();
}

async function runAll() {
  const testCases = [];
  if (/benchmark=true/.test(location.search)) {
    testCases.push(TestDOMUpdater);
  }
  else {
    testCases.push(
      TestContextMenu,
      TestHidden,
      TestNewTab,
      TestSuccessor,
      TestTree,
      TestCloseParentBehavior
    );
  }
  let runOnlyRunnable = false;
  findRunnable:
  for (const tests of testCases) {
    for (const name of Object.keys(tests)) {
      if (!name.startsWith('test'))
        continue;
      if (tests[name].runnable) {
        runOnlyRunnable = true;
        break findRunnable;
      }
    }
  }
  for (const tests of testCases) {
    const setup    = tests.setUp || tests.setup;
    const teardown = tests.tearDown || tests.teardown;
    for (const name of Object.keys(tests)) {
      if (!name.startsWith('test'))
        continue;
      if (runOnlyRunnable && !tests[name].runnable)
        continue;
      await restoreConfigs(configs.$default);
      let shouldTearDown = true;
      const result = mResults.appendChild(document.createElement('span'));
      try {
        if (typeof setup == 'function')
          await setup();
        await tests[name]();
        if (typeof teardown == 'function') {
          await teardown();
          shouldTearDown = false;
        }
        result.classList.add('success');
        result.setAttribute('title', `Success: ${name}`);
        result.textContent = '.';
      }
      catch(error) {
        try {
          if (shouldTearDown &&
              typeof teardown == 'function') {
            await teardown();
          }
          throw error;
        }
        catch(error) {
          if (error && error.name == 'AssertionError') {
            logFailure(name, error);
            result.classList.add('failure');
            result.setAttribute('title', `Failure: ${name}`);
            result.textContent = 'F';
          }
          else {
            logError(name, error);
            result.classList.add('error');
            result.setAttribute('title', `Error: ${name}`);
            result.textContent = 'E';
          }
        }
      }
    }
  }
  const result = mResults.appendChild(document.createElement('span'));
  result.textContent = 'Done.';
}

function logError(name, error) {
  const item = mLogs.appendChild(document.createElement('li'));
  item.classList.add('error');
  const description = item.appendChild(document.createElement('div'));
  description.classList.add('description');
  description.textContent = name;
  if (error) {
    description.appendChild(document.createElement('br'));
    description.appendChild(document.createTextNode(error.toString()));

    const stack = item.appendChild(document.createElement('pre'));
    stack.classList.add('stack');
    stack.textContent = error.stack;
  }
}

function logFailure(name, error) {
  const item = mLogs.appendChild(document.createElement('li'));
  item.classList.add('failure');
  const description = item.appendChild(document.createElement('div'));
  description.classList.add('description');
  description.textContent = name;
  if (error.message) {
    description.appendChild(document.createElement('br'));
    description.appendChild(document.createTextNode(error.message));
  }

  const stack = item.appendChild(document.createElement('pre'));
  stack.classList.add('stack');
  stack.textContent = error.stack;

  if ('expected' in error) {
    const expectedBlock = item.appendChild(document.createElement('fieldset'));
    expectedBlock.appendChild(document.createElement('legend')).textContent = 'Expected';
    const expected = expectedBlock.appendChild(document.createElement('pre'));
    expected.classList.add('expected');
    expected.textContent = (error.expected || '').trim();
  }

  const actualBlock = item.appendChild(document.createElement('fieldset'));
  actualBlock.appendChild(document.createElement('legend')).textContent = 'Actual';
  const actual = actualBlock.appendChild(document.createElement('pre'));
  actual.classList.add('actual');
  actual.textContent = (error.actual || '').trim();

  if ('expected' in error) {
    const diffBlock = item.appendChild(document.createElement('fieldset'));
    diffBlock.appendChild(document.createElement('legend')).textContent = 'Difference';
    const diff = diffBlock.appendChild(document.createElement('pre'));
    diff.classList.add('diff');
    const range = document.createRange();
    range.selectNodeContents(diff);
    range.collapse(false);
    diff.appendChild(range.createContextualFragment(Diff.readable(error.expected, error.actual, true)));
    range.detach();
  }
}

window.addEventListener('DOMContentLoaded', run, { once: true });
