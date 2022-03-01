/*
 license: The MIT License, Copyright (c) 2020 YUKI "Piro" Hiroshi
*/
'use strict';

import { is /*, ok, ng*/ } from '/tests/assert.js';

import { DOMUpdater } from '/extlib/dom-updater.js';
import morphdom from '/node_modules/morphdom/dist/morphdom-esm.js';

const container = document.body.appendChild(document.createElement('div'));

document.body.appendChild(document.createElement('h1')).textContent = 'Benchmark DOMUpdater vs morphdom';
const result = document.body.appendChild(document.createElement('ul'));

function createNode(source) {
  const node = document.createElement('div');
  node.appendChild(createFragment(source));
  return node;
}

function createFragment(source) {
  const range = document.createRange();
  range.setStart(document.body, 0);
  const contents = range.createContextualFragment(source.trim());
  range.detach();
  return contents;
}

function assertUpdatedWithDOMUpdater(from, to) {
  const fromNode = createNode(from);
  container.appendChild(fromNode);
  const start = Date.now();
  DOMUpdater.update(fromNode, createFragment(to));
  const end = Date.now();
  is(createNode(to).innerHTML, fromNode.innerHTML);
  return end - start;
}

function assertUpdatedWithMorphdomString(from, to) {
  const fromNode = createNode(from);
  container.appendChild(fromNode);
  const start = Date.now();
  morphdom(fromNode, `<div>${to}</div>`);
  const end = Date.now();
  is(createNode(to).innerHTML, fromNode.innerHTML);
  return end - start;
}

function assertUpdatedWithMorphdomDocumentFragment(from, to) {
  const fromNode = createNode(from);
  container.appendChild(fromNode);
  const start = Date.now();
  morphdom(fromNode, createNode(to));
  const end = Date.now();
  is(createNode(to).innerHTML, fromNode.innerHTML);
  return end - start;
}

async function benchamrk(from, to) {
  const range = document.createRange();
  const tries = 5000;
  let totalDOMUpdater = 0;
  let totalMorphdomString = 0;
  let totalMorphdomDocumentFragment = 0;
  const tasks = [
    (() => {
      range.selectNodeContents(container);
      range.deleteContents();
      totalDOMUpdater += assertUpdatedWithDOMUpdater(from, to);
    }),
    (() => {
      range.selectNodeContents(container);
      range.deleteContents();
      totalMorphdomString += assertUpdatedWithMorphdomString(from, to);
    }),
    (() => {
      range.selectNodeContents(container);
      range.deleteContents();
      totalMorphdomDocumentFragment += assertUpdatedWithMorphdomDocumentFragment(from, to);
    })
  ];
  let start = 0;
  for (let i = 0; i < tries; i++) {
    // shuffle with Fisher–Yates
    for (let i = tasks.length - 1; i > 0; i--) {
      const randomIndex = Math.floor(Math.random() * (i + 1));
      const element = tasks[i];
      tasks[i] = tasks[randomIndex];
      tasks[randomIndex] = element;
    }
    for (const task of tasks) {
      task();
    }
    if (Date.now() - start > 3000) {
      console.log('3sec past, wait for next tick');
      await new Promise(resolve => setTimeout(resolve, 0));
      start = Date.now();
    }
  }
  range.selectNodeContents(container);
  range.deleteContents();
  range.detach();
  result.appendChild(document.createElement('li')).textContent = `DOMUpdater: DocumentFragment ${totalDOMUpdater}`;
  result.appendChild(document.createElement('li')).textContent = `morphdom: string ${totalMorphdomString}`;
  result.appendChild(document.createElement('li')).textContent = `morphdom: DocumentFragment ${totalMorphdomDocumentFragment}`;
}

export async function testBenchmark() {
  await benchamrk(
    `
      <span id="item1">contents
        <span id="item1-1">contents</span>
        <span id="item1-2">contents</span>
        <span id="item1-3" part="active">contents, active</span>
        <span id="item1-4">contents</span>
        <span id="item1-5">contents</span>
        <span id="item1-6">contents</span>
      </span>
      <span id="item2">contents
        <span id="item2-1">contents</span>
        <span id="item2-2">contents</span>
        <span id="item2-3" part="active">contents, active</span>
        <span id="item2-4">contents</span>
        <span id="item2-5">contents</span>
        <span id="item2-6">contents</span>
      </span>
      <span id="item3" part="active">contents, active
        <span id="item3-1">contents</span>
        <span id="item3-2">contents</span>
        <span id="item3-3" part="active">contents, active</span>
        <span id="item3-4">contents</span>
        <span id="item3-5">contents</span>
        <span id="item3-6">contents</span>
      </span>
      <span id="item4">contents
        <span id="item4-1">contents</span>
        <span id="item4-2">contents</span>
        <span id="item4-3" part="active">contents, active</span>
        <span id="item4-4">contents</span>
        <span id="item4-5">contents</span>
        <span id="item4-6">contents</span>
      </span>
      <span id="item5">contents
        <span id="item5-1">contents</span>
        <span id="item5-2">contents</span>
        <span id="item5-3" part="active">contents, active</span>
        <span id="item5-4">contents</span>
        <span id="item5-5">contents</span>
        <span id="item5-6">contents</span>
      </span>
      <span id="item6">contents
        <span id="item6-1">contents</span>
        <span id="item6-2">contents</span>
        <span id="item6-3" part="active">contents, active</span>
        <span id="item6-4">contents</span>
        <span id="item6-5">contents</span>
        <span id="item6-6">contents</span>
      </span>
    `.trim(),
    `
      <span id="item3">contents, old active
        <span id="item3-3">contents, old active</span>
        <span id="item3-4">contents</span>
        <span id="item3-5">contents</span>
        <span id="item3-6" part="active">contents, new active</span>
        <span id="item3-7">contents</span>
        <span id="item3-8">contents</span>
      </span>
      <span id="item4">contents
        <span id="item4-3">contents, old active</span>
        <span id="item4-4">contents</span>
        <span id="item4-5">contents</span>
        <span id="item4-6" part="active">contents, new active</span>
        <span id="item4-7">contents</span>
        <span id="item4-8">contents</span>
      </span>
      <span id="item5">contents
        <span id="item5-3">contents, old active</span>
        <span id="item5-4">contents</span>
        <span id="item5-5">contents</span>
        <span id="item5-6" part="active">contents, new active</span>
        <span id="item5-7">contents</span>
        <span id="item5-8">contents</span>
      </span>
      <span id="item6" part="active">contents, new active
        <span id="item6-3">contents, old active</span>
        <span id="item6-4">contents</span>
        <span id="item6-5">contents</span>
        <span id="item6-6" part="active">contents, new active</span>
        <span id="item6-7">contents</span>
        <span id="item6-8">contents</span>
      </span>
      <span id="item7">contents</span>
      <span id="item8">contents</span>
    `.trim()
  );
}
