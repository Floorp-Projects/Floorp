// SPDX-License-Identifier: MPL-2.0

export class M0RecordCommitFailure extends Error {
  constructor(readonly uncertain: boolean) {
    super("record_commit_failed");
    this.name = "M0RecordCommitFailure";
  }
}
