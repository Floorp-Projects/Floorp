// SPDX-License-Identifier: MPL-2.0
// A public API program, shared verbatim by document and genuine Worker fixtures.
export const managedStorageProbeSource = String.raw`
globalThis.runManagedStorageProbe = async function() {
  const observations = [];
  const read = async (name, operation) => {
    try { observations.push({name, value: await operation()}); }
    catch (error) { observations.push({name, error: String(error?.message || error)}); }
  };
  await read('all', () => chrome.storage.managed.get(null));
  await read('string', () => chrome.storage.managed.get('missing'));
  await read('array', () => chrome.storage.managed.get(['missing', 'other']));
  await read('defaults', () => chrome.storage.managed.get({missing: 'fallback', count: 7, nested: {fixed: true}}));
  await read('keys', () => chrome.storage.managed.getKeys());
  await read('bytes', () => chrome.storage.managed.getBytesInUse(null));
  await new Promise(resolve => chrome.storage.managed.get(null, value => {
    const error = chrome.runtime.lastError;
    observations.push({name: 'callback', ...(error ? {error: error.message} : {value})});
    resolve();
  }));
  for (const name of ['set', 'remove', 'clear']) {
    await read(name, () => name === 'set' ? chrome.storage.managed.set({missing: 'written'}) :
      name === 'remove' ? chrome.storage.managed.remove('missing') : chrome.storage.managed.clear());
  }
  await read('afterWrites', () => chrome.storage.managed.get(null));
  return {observations, publicId: chrome.runtime.id,
    worker: typeof ServiceWorkerGlobalScope !== 'undefined' && self instanceof ServiceWorkerGlobalScope,
    hasDocument: typeof document !== 'undefined', module: globalThis.managedModule === true};
};
`;
