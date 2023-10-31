/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use authenticator::{InteractiveRequest, InteractiveUpdate};

pub(crate) type InteractiveManagementReceiver = Option<Sender<InteractiveRequest>>;
pub(crate) fn send_about_prompt(prompt: &BrowserPromptType) -> Result<(), nsresult> {
    let json = nsString::from(&serde_json::to_string(&prompt).unwrap_or_default());
    notify_observers(PromptTarget::AboutPage, json)
}

pub(crate) fn interactive_status_callback(
    status_rx: Receiver<StatusUpdate>,
    transaction: Arc<Mutex<Option<TransactionState>>>, /* Shared with an AuthrsTransport */
) -> Result<(), nsresult> {
    loop {
        match status_rx.recv() {
            Ok(StatusUpdate::InteractiveManagement(InteractiveUpdate::StartManagement((
                tx,
                auth_info,
            )))) => {
                let mut guard = transaction.lock().unwrap();
                let Some(transaction) = guard.as_mut() else {
                    warn!("STATUS: received status update after end of transaction.");
                    break;
                };
                transaction.interactive_receiver.replace(tx);
                let prompt = BrowserPromptType::SelectedDevice { auth_info };
                send_about_prompt(&prompt)?;
            }
            Ok(StatusUpdate::SelectDeviceNotice) => {
                info!("STATUS: Please select a device by touching one of them.");
                let prompt = BrowserPromptType::SelectDevice;
                send_about_prompt(&prompt)?;
            }
            Ok(_) => {
                // currently not handled
                continue;
            }
            Err(RecvError) => {
                info!("STATUS: end");
                break;
            }
        }
    }
    Ok(())
}
