// SPDX-License-Identifier: MPL-2.0
/** Evidence from the actual extension realm and persisted native filesystem. */
export interface ConcurrentFileSystemResult {
  concurrency: {
    pendingAtStart: number;
    records: {
      name: string;
      expectedSize: number;
      expectedSHA256: string;
      fileSize: number;
      fileSHA256: string;
      fetchSize: number;
      fetchSHA256: string;
      nativeFile: boolean;
      events: string[];
      progress: { loaded: number; total: number }[];
      state: number;
      position: number;
      length: number;
      errorCode: number | null;
    }[];
    cancelled: {
      stateBeforeAbort: number;
      state: number;
      errorCode: number | null;
      events: string[];
      position: number;
      length: number;
      text: string;
      fetched: string;
    };
    reused: {
      text: string;
      fetched: string;
      state: number;
      errorCode: number | null;
    };
    rewritten: {
      size: number;
      nativeFile: boolean;
      text: string;
      fetched: string;
    };
  };
}

export interface PendingFileSystemWrites {
  pendingWrites: number;
}
