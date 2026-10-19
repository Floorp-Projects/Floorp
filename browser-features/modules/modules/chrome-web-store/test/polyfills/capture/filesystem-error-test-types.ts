// SPDX-License-Identifier: MPL-2.0
/** Public callback observations from the converted extension's actual realm. */
export interface FileSystemPublicError {
  name: string;
  code: number;
  message: string;
  constructorName: string;
  tag: string;
  ownProperties: string[];
  nativeException: boolean;
  nativeConstructor: boolean;
}

export interface FileSystemErrorResult {
  errorContract: {
    missingFile: {
      successCalls: number;
      errorCalls: number;
      returnType: string;
      error: FileSystemPublicError | null;
    };
    missingDirectory: {
      successCalls: number;
      errorCalls: number;
      returnType: string;
      error: FileSystemPublicError | null;
    };
    existsStatRecovered: boolean;
    missingRead: FileSystemPublicError | null;
    mappedVendorError: string | null;
    readAfterCreate: string;
    legacyNotFoundConstant: number;
    legacySyntaxConstant: number;
  };
}
