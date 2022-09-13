/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import * as Constants from '/common/constants.js';

window.addEventListener('DOMContentLoaded', () => {
  browser.runtime.onMessage.addListener((message, _sender) => {
    if (!message ||
        typeof message != 'object')
      return;
    switch (message.type) {
      case Constants.kCOMMAND_RESPONSE_QUERY_LOGS: {
        if (!message.logs || message.logs.length < 1)
          return;
        document.getElementById('queryLogs').textContent += message.logs.reduce((output, log, index) => {
          if (log) {
            log.windowId = message.windowId || 'background';
            output += `${index == 0 ? '' : ',\n'}${JSON.stringify(log)}`;
          }
          return output;
        }, '') + ',\n';
        analyzeQueryLogs();
      }; break;

      case Constants.kCOMMAND_RESPONSE_CONNECTION_MESSAGE_LOGS: {
        const logs = document.getElementById('connectionMessageLogs');
        logs.textContent += message.windowId ? `"${message.windowId} => background"` : '"background => sidebar"';
        logs.textContent += ': ' + JSON.stringify(message.logs, null, 2) + ',\n';
        analyzeConnectionMessageLogs();
      }; break;
    }
  });
  browser.runtime.sendMessage({ type: Constants.kCOMMAND_REQUEST_QUERY_LOGS });
  browser.runtime.sendMessage({ type: Constants.kCOMMAND_REQUEST_CONNECTION_MESSAGE_LOGS });
}, { once: true });

function analyzeQueryLogs() {
  const logs = JSON.parse(`[${document.getElementById('queryLogs').textContent.replace(/,\s*$/, '')}]`);

  function toString(data) {
    return JSON.stringify(data);
  }

  function fromString(data) {
    return JSON.parse(data);
  }

  const totalElapsedTimes = {};
  function normalize(log) {
    const elapsedTime = log.elapsed || log.elasped || 0;
    log = fromString(toString(log));
    delete log.elasped;
    delete log.elapsed;
    delete log.tabs;
    log.source = (typeof log.windowId == 'number') ? 'sidebar' : log.windowId;
    delete log.windowId;
    if (log.indexedTabs)
      log.indexedTabs = log.indexedTabs.replace(/\s+in window \d+$/i, '');
    if (log.fromId)
      log.fromId = 'given';
    if (log.toId)
      log.toId = 'given';
    if (log.fromIndex)
      log.fromIndex = 'given';
    if (log.logicalIndex)
      log.logicalIndex = 'given';
    for (const key of ['id', '!id']) {
      if (log[key])
        log[key] = Array.isArray(log[key]) ? 'array' : (typeof log[key]);
    }
    const sorted = {};
    for (const key of Object.keys(log).sort()) {
      sorted[key] = log[key];
    }
    const type = toString(sorted);
    const total = totalElapsedTimes[type] || 0;
    totalElapsedTimes[type] = total + elapsedTime;
    return sorted;
  }
  const normalizedLogs = logs.map(normalize).map(toString).sort().map(fromString);

  function uniq(logs) {
    const logTypes = logs.map(toString);
    let lastType;
    let lastCount;
    const results = [];
    for (const type of logTypes) {
      if (type != lastType) {
        if (lastType) {
          results.push({
            count: lastCount,
            query: fromString(lastType),
            totalElapsed: totalElapsedTimes[lastType]
          });
        }
        lastType = type;
        lastCount = 0;
      }
      lastCount++;
    }
    if (lastType)
      results.push({
        count: lastCount,
        query: fromString(lastType),
        totalElapsed: totalElapsedTimes[lastType]
      });
    return results;
  }

  const results = [];
  results.push('Top 10 slowest queries:\n' + logs.sort((a,b) => (b.elasped || b.elapsed || 0) - (a.elasped || a.elapsed || 0)).slice(0, 10).map(toString).join('\n'));
  results.push('Count of query tyepes:\n' + uniq(normalizedLogs).sort((a, b) => b.count - a.count).map(toString).join('\n'));
  results.push('Sorted in total elapsed time:\n' + uniq(normalizedLogs).sort((a, b) => b.totalElapsed - a.totalElapsed).map(toString).join('\n'));
  document.getElementById('queryLogsAnalysis').textContent = '`\n' + results.join('\n') + '\n`';
}

function analyzeConnectionMessageLogs() {
  const logs = JSON.parse(`{${document.getElementById('connectionMessageLogs').textContent.replace(/,\s*$/, '')}}`);

  const counts = {};
  for (const direction of Object.keys(logs)) {
    let partialLogs = logs[direction];
    const isBackground = direction.startsWith('background');
    if (!isBackground) {
      partialLogs = {};
      partialLogs[direction] = logs[direction];
    }
    for (const part of Object.keys(partialLogs)) {
      for (const type of Object.keys(partialLogs[part])) {
        const typeLabel = `${type} (${isBackground ? 'background => sidebar' : 'sidebar => background'})`;
        counts[typeLabel] = counts[typeLabel] || 0;
        counts[typeLabel] += partialLogs[part][type];
      }
    }
  }

  let totalCount = 0;
  const sortableCounts = [];
  for (const type of Object.keys(counts)) {
    sortableCounts.push({ type, count: counts[type] });
    totalCount += counts[type];
  }

  const results = [];
  results.push('Top 10 message types:\n' + sortableCounts.sort((a,b) => b.count- a.count).slice(0, 10).map(count => `${count.type}: ${count.count} (${parseInt(count.count / totalCount * 100)} %)`).join('\n'));
  document.getElementById('connectionMessageLogsAnalysis').textContent = '`\n' + results.join('\n') + '\n`';
}
