// SPDX-License-Identifier: MPL-2.0
// A public API program, shared verbatim by document and genuine Worker fixtures.
export const managedStorageProbeSource = String.raw`
globalThis.runManagedStorageProbe = async function(legacyCallbacks = false) {
  const observations = [];
  const read = async (name, operation, callbackOperation) => {
    try {
      const value = await (legacyCallbacks ? new Promise((resolve, reject) => {
        callbackOperation(value => {
          const error = chrome.runtime.lastError;
          if (error) reject(new Error(error.message));
          else resolve(value);
        });
      }) : operation());
      observations.push({name, value});
    } catch (error) { observations.push({name, error: String(error?.message || error)}); }
  };
  await read('all', () => chrome.storage.managed.get(null), callback => chrome.storage.managed.get(null, callback));
  await read('string', () => chrome.storage.managed.get('missing'), callback => chrome.storage.managed.get('missing', callback));
  await read('array', () => chrome.storage.managed.get(['missing', 'other']), callback => chrome.storage.managed.get(['missing', 'other'], callback));
  await read('defaults', () => chrome.storage.managed.get({missing: 'fallback', count: 7, nested: {fixed: true}}), callback => chrome.storage.managed.get({missing: 'fallback', count: 7, nested: {fixed: true}}, callback));
  await read('keys', () => chrome.storage.managed.getKeys(), callback => chrome.storage.managed.getKeys(callback));
  await read('bytes', () => chrome.storage.managed.getBytesInUse(null), callback => chrome.storage.managed.getBytesInUse(null, callback));
  await new Promise(resolve => chrome.storage.managed.get(null, value => {
    const error = chrome.runtime.lastError;
    observations.push({name: 'callback', ...(error ? {error: error.message} : {value})});
    resolve();
  }));
  for (const name of ['set', 'remove', 'clear']) {
    await read(name, () => name === 'set' ? chrome.storage.managed.set({missing: 'written'}) :
      name === 'remove' ? chrome.storage.managed.remove('missing') : chrome.storage.managed.clear(),
      callback => name === 'set' ? chrome.storage.managed.set({missing: 'written'}, callback) :
      name === 'remove' ? chrome.storage.managed.remove('missing', callback) : chrome.storage.managed.clear(callback));
  }
  await read('afterWrites', () => chrome.storage.managed.get(null), callback => chrome.storage.managed.get(null, callback));
  return {observations, legacyCallbacks, publicId: chrome.runtime.id,
    worker: typeof ServiceWorkerGlobalScope !== 'undefined' && self instanceof ServiceWorkerGlobalScope,
    hasDocument: typeof document !== 'undefined', module: globalThis.managedModule === true};
};
`;
