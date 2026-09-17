// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals as harnessAssertEquals,
  runTests,
  type TestCase,
} from "../../../../../chrome/test/utils/test_harness.ts";
import { prepareUploadFile } from "./UploadFileReader.sys.mts";

function assertEquals<T>(actual: T, expected: T): void {
  harnessAssertEquals(actual, expected, "values should be equal");
}

async function testReadsExistingFile(): Promise<void> {
  const result = await prepareUploadFile("C:\\tmp\\upload.txt", {
    exists: () => Promise.resolve(true),
    read: () => Promise.resolve(new Uint8Array([1, 2, 255])),
    filename: () => "upload.txt",
  });

  assertEquals(result?.fileName, "upload.txt");
  assertEquals(JSON.stringify(result?.fileData), JSON.stringify([1, 2, 255]));
}

async function testMissingFileReturnsNull(): Promise<void> {
  let readCalled = false;
  const result = await prepareUploadFile("C:\\tmp\\missing.txt", {
    exists: () => Promise.resolve(false),
    read: () => {
      readCalled = true;
      return Promise.resolve(new Uint8Array());
    },
    filename: () => "missing.txt",
  });

  assertEquals(result, null);
  assertEquals(readCalled, false);
}

async function testEmptyPathReturnsNull(): Promise<void> {
  let existsCalled = false;
  const result = await prepareUploadFile("  ", {
    exists: () => {
      existsCalled = true;
      return Promise.resolve(true);
    },
    read: () => Promise.resolve(new Uint8Array()),
    filename: () => "upload.txt",
  });

  assertEquals(result, null);
  assertEquals(existsCalled, false);
}

async function testReadFailureReturnsNull(): Promise<void> {
  const result = await prepareUploadFile("C:\\tmp\\upload.txt", {
    exists: () => Promise.resolve(true),
    read: () => Promise.reject(new Error("read failed")),
    filename: () => "upload.txt",
  });

  assertEquals(result, null);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "reads an existing upload file", fn: testReadsExistingFile },
    { name: "does not read a missing file", fn: testMissingFileReturnsNull },
    { name: "rejects an empty file path", fn: testEmptyPathReturnsNull },
    { name: "handles a read failure", fn: testReadFailureReturnsNull },
  ];
  await runTests("UploadFileReader.test.mts", tests);
}
