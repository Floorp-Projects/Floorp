/**
 * Utilities to improve the performance of the CTS, by caching data that is
 * expensive to build using a two-level cache (in-memory, pre-computed file).
 */

interface DataStore {
  load(path: string): Promise<string>;
}

/** Logger is a basic debug logger function */
export type Logger = (s: string) => void;

/** DataCache is an interface to a data store used to hold cached data */
export class DataCache {
  /** setDataStore() sets the backing data store used by the data cache */
  public setStore(dataStore: DataStore) {
    this.dataStore = dataStore;
  }

  /** setDebugLogger() sets the verbose logger */
  public setDebugLogger(logger: Logger) {
    this.debugLogger = logger;
  }

  /**
   * fetch() retrieves cacheable data from the data cache, first checking the
   * in-memory cache, then the data store (if specified), then resorting to
   * building the data and storing it in the cache.
   */
  public async fetch<Data>(cacheable: Cacheable<Data>): Promise<Data> {
    // First check the in-memory cache
    let data = this.cache.get(cacheable.path);
    if (data !== undefined) {
      this.log('in-memory cache hit');
      return Promise.resolve(data as Data);
    }
    this.log('in-memory cache miss');
    // In in-memory cache miss.
    // Next, try the data store.
    if (this.dataStore !== null && !this.unavailableFiles.has(cacheable.path)) {
      let serialized: string | undefined;
      try {
        serialized = await this.dataStore.load(cacheable.path);
        this.log('loaded serialized');
      } catch (err) {
        // not found in data store
        this.log(`failed to load (${cacheable.path}): ${err}`);
        this.unavailableFiles.add(cacheable.path);
      }
      if (serialized !== undefined) {
        this.log(`deserializing`);
        data = cacheable.deserialize(serialized);
        this.cache.set(cacheable.path, data);
        return data as Data;
      }
    }
    // Not found anywhere. Build the data, and cache for future lookup.
    this.log(`cache: building (${cacheable.path})`);
    data = await cacheable.build();
    this.cache.set(cacheable.path, data);
    return data as Data;
  }

  private log(msg: string) {
    if (this.debugLogger !== null) {
      this.debugLogger(`DataCache: ${msg}`);
    }
  }

  private cache = new Map<string, unknown>();
  private unavailableFiles = new Set<string>();
  private dataStore: DataStore | null = null;
  private debugLogger: Logger | null = null;
}

/** The data cache */
export const dataCache = new DataCache();

/** true if the current process is building the cache */
let isBuildingDataCache = false;

/** @returns true if the data cache is currently being built */
export function getIsBuildingDataCache() {
  return isBuildingDataCache;
}

/** Sets whether the data cache is currently being built */
export function setIsBuildingDataCache(value = true) {
  isBuildingDataCache = value;
}

/**
 * Cacheable is the interface to something that can be stored into the
 * DataCache.
 * The 'npm run gen_cache' tool will look for module-scope variables of this
 * interface, with the name `d`.
 */
export interface Cacheable<Data> {
  /** the globally unique path for the cacheable data */
  readonly path: string;

  /**
   * build() builds the cacheable data.
   * This is assumed to be an expensive operation and will only happen if the
   * cache does not already contain the built data.
   */
  build(): Promise<Data>;

  /**
   * serialize() transforms `data` to a string (usually JSON encoded) so that it
   * can be stored in a text cache file.
   */
  serialize(data: Data): string;

  /**
   * deserialize() is the inverse of serialize(), transforming the string back
   * to the Data object.
   */
  deserialize(serialized: string): Data;
}
