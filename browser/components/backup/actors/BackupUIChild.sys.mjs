/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A JSWindowActor that is responsible for marshalling information between
 * the BackupService singleton and any registered UI widgets that need to
 * represent data from that service. Any UI widgets that want to receive
 * state updates from BackupService should emit a BackupUI:InitWidget
 * event in a document that this actor pair is registered for.
 */
export class BackupUIChild extends JSWindowActorChild {
  #inittedWidgets = new WeakSet();

  /**
   * Handles custom events fired by widgets that want to register with
   * BackupUIChild.
   *
   * @param {Event} event
   *   The custom event that the widget fired.
   */
  handleEvent(event) {
    /**
     * BackupUI:InitWidget sends a message to the parent to request the BackupService state
     * which will result in a `backupServiceState` property of the widget to be set when that
     * state is received. Subsequent state updates will also cause that state property to
     * be set.
     */
    if (event.type == "BackupUI:InitWidget") {
      this.#inittedWidgets.add(event.target);
      this.sendAsyncMessage("RequestState");
    } else if (event.type == "BackupUI:ScheduledBackupsConfirm") {
      this.sendAsyncMessage("ScheduledBackupsConfirm");
    }
  }

  /**
   * Handles messages sent by BackupUIParent.
   *
   * @param {ReceiveMessageArgument} message
   *   The message received from the BackupUIParent.
   */
  receiveMessage(message) {
    if (message.name == "StateUpdate") {
      let widgets = ChromeUtils.nondeterministicGetWeakSetKeys(
        this.#inittedWidgets
      );
      for (let widget of widgets) {
        if (widget.isConnected) {
          // Note: we might need to switch to using Cu.cloneInto here in the
          // event that these widgets are embedded in a non-parent-process
          // context, like in an onboarding card.
          widget.backupServiceState = message.data.state;
        }
      }
    }
  }
}
