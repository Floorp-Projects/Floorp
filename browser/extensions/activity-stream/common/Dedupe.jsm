this.Dedupe = class Dedupe {
  constructor(createKey, compare) {
    this.createKey = createKey || this.defaultCreateKey;
    this.compare = compare || this.defaultCompare;
  }

  defaultCreateKey(item) {
    return item;
  }

  defaultCompare() {
    return false;
  }

  /**
   * Dedupe an array containing groups of elements.
   * Duplicate removal favors earlier groups.
   *
   * @param {Array} groups Contains an arbitrary number of arrays of elements.
   * @returns {Array}
   */
  group(groups) {
    const globalKeys = new Set();
    const result = [];
    for (const values of groups) {
      const valueMap = new Map();
      for (const value of values) {
        const key = this.createKey(value);
        if (!globalKeys.has(key) && (!valueMap.has(key) || this.compare(valueMap.get(key), value))) {
          valueMap.set(key, value);
        }
      }
      result.push(valueMap);
      valueMap.forEach((value, key) => globalKeys.add(key));
    }
    return result.map(m => Array.from(m.values()));
  }
};

this.EXPORTED_SYMBOLS = ["Dedupe"];
