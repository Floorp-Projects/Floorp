var localStorageMock = (() => {
  let store = {};
  return {
    getItem: (key) => {
      if (!store.hasOwnProperty(key)) {
        return "";
      }
      return store[key];
    },
    setItem: (key, value) => {
      store[key] = value.toString();
    },
    clear: () => {
      store = {};
    },
    removeItem: (key) => {
      delete store[key];
    },
    key: (index) => {
      return Object.keys(store)[index];
    },
    get length() {
      return Object.keys(store).length;
    }
  };
})();

Object.defineProperty(window, "localStorage", {
  value: localStorageMock
});
