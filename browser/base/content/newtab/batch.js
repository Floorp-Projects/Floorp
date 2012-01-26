#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * This class makes it easy to wait until a batch of callbacks has finished.
 *
 * Example:
 *
 * let batch = new Batch(function () alert("finished"));
 * let pop = batch.pop.bind(batch);
 *
 * for (let i = 0; i < 5; i++) {
 *   batch.push();
 *   setTimeout(pop, i * 1000);
 * }
 *
 * batch.close();
 */
function Batch(aCallback) {
  this._callback = aCallback;
}

Batch.prototype = {
  /**
   * The number of batch entries.
   */
  _count: 0,

  /**
   * Whether this batch is closed.
   */
  _closed: false,

  /**
   * Increases the number of batch entries by one.
   */
  push: function Batch_push() {
    if (!this._closed)
      this._count++;
  },

  /**
   * Decreases the number of batch entries by one.
   */
  pop: function Batch_pop() {
    if (this._count)
      this._count--;

    if (this._closed)
      this._check();
  },

  /**
   * Closes the batch so that no new entries can be added.
   */
  close: function Batch_close() {
    if (this._closed)
      return;

    this._closed = true;
    this._check();
  },

  /**
   * Checks if the batch has finished.
   */
  _check: function Batch_check() {
    if (this._count == 0 && this._callback) {
      this._callback();
      this._callback = null;
    }
  }
};
