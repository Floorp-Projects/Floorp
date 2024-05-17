/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BackupService: "resource:///modules/backup/BackupService.sys.mjs",
});

/**
 * A JSWindowActor that is responsible for marshalling information between
 * the BackupService singleton and any registered UI widgets that need to
 * represent data from that service.
 */
export class BackupUIParent extends JSWindowActorParent {
  /**
   * A reference to the BackupService singleton instance.
   *
   * @type {BackupService}
   */
  #bs;

  /**
   * Create a BackupUIParent instance. If a BackupUIParent is instantiated
   * before BrowserGlue has a chance to initialize the BackupService, this
   * constructor will cause it to initialize first.
   */
  constructor() {
    super();
    // We use init() rather than get(), since it's possible to load
    // about:preferences before the service has had a chance to init itself
    // via BrowserGlue.
    this.#bs = lazy.BackupService.init();
  }

  /**
   * Called once the BackupUIParent/BackupUIChild pair have been connected.
   */
  actorCreated() {
    this.#bs.addEventListener("BackupService:StateUpdate", this);
  }

  /**
   * Called once the BackupUIParent/BackupUIChild pair have been disconnected.
   */
  didDestroy() {
    this.#bs.removeEventListener("BackupService:StateUpdate", this);
  }

  /**
   * Handles events fired by the BackupService.
   *
   * @param {Event} event
   *   The event that the BackupService emitted.
   */
  handleEvent(event) {
    if (event.type == "BackupService:StateUpdate") {
      this.sendState();
    }
  }

  /**
   * Handles messages sent by BackupUIChild.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the BackupUIChild.
   */
  receiveMessage(message) {
    if (message.name == "RequestState") {
      this.sendState();
    } else if (message.name == "ScheduledBackupsConfirm") {
      this.#bs.setScheduledBackups(true);
    }
  }

  /**
   * Sends the StateUpdate message to the BackupUIChild, along with the most
   * recent state object from BackupService.
   */
  sendState() {
    this.sendAsyncMessage("StateUpdate", { state: this.#bs.state });
  }
}
