/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";
import { ModalOverlayWrapper } from "content-src/components/ModalOverlay/ModalOverlay";

export class DSPrivacyModal extends React.PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.onLearnLinkClick = this.onLearnLinkClick.bind(this);
    this.onManageLinkClick = this.onManageLinkClick.bind(this);
  }

  onLearnLinkClick() {
    this.props.dispatch(
      ac.DiscoveryStreamUserEvent({
        event: "CLICK_PRIVACY_INFO",
        source: "DS_PRIVACY_MODAL",
      })
    );
  }

  onManageLinkClick() {
    this.props.dispatch(ac.OnlyToMain({ type: at.SETTINGS_OPEN }));
  }

  closeModal() {
    this.props.dispatch({
      type: `HIDE_PRIVACY_INFO`,
      data: {},
    });
  }

  render() {
    return (
      <ModalOverlayWrapper
        onClose={this.closeModal}
        innerClassName="ds-privacy-modal"
      >
        <div className="privacy-notice">
          <h3 data-l10n-id="newtab-privacy-modal-header" />
          <p data-l10n-id="newtab-privacy-modal-paragraph-2" />
          <a
            className="modal-link modal-link-privacy"
            data-l10n-id="newtab-privacy-modal-link"
            onClick={this.onLearnLinkClick}
            href="https://support.mozilla.org/kb/pocket-recommendations-firefox-new-tab"
          />
          <button
            className="modal-link modal-link-manage"
            data-l10n-id="newtab-privacy-modal-button-manage"
            onClick={this.onManageLinkClick}
          />
        </div>
        <section className="actions">
          <button
            className="done"
            type="submit"
            onClick={this.closeModal}
            data-l10n-id="newtab-privacy-modal-button-done"
          />
        </section>
      </ModalOverlayWrapper>
    );
  }
}
