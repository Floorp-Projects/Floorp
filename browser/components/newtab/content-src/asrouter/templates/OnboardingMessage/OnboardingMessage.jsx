import { ModalOverlay } from "../../components/ModalOverlay/ModalOverlay";
import React from "react";

const FLUENT_FILES = [
  "branding/brand.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
];

export class OnboardingCard extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    const { props } = this;
    const ping = {
      event: "CLICK_BUTTON",
      message_id: props.id,
      id: props.UISurface,
    };
    props.sendUserActionTelemetry(ping);
    props.onAction(props.content.primary_button.action);
  }

  render() {
    const { content } = this.props;
    const className = this.props.className || "onboardingMessage";
    return (
      <div className={className}>
        <div className={`onboardingMessageImage ${content.icon}`} />
        <div className="onboardingContent">
          <span>
            <h3
              className="onboardingTitle"
              data-l10n-id={content.title.string_id}
            />
            <p
              className="onboardingText"
              data-l10n-id={content.text.string_id}
            />
          </span>
          <span className="onboardingButtonContainer">
            <button
              data-l10n-id={content.primary_button.label.string_id}
              className="button onboardingButton"
              onClick={this.onClick}
            />
          </span>
        </div>
      </div>
    );
  }
}

export class OnboardingMessage extends React.PureComponent {
  componentWillMount() {
    FLUENT_FILES.forEach(file => {
      const link = document.head.appendChild(document.createElement("link"));
      link.href = file;
      link.rel = "localization";
    });
  }

  render() {
    const { props } = this;
    const { button_label, header } = props.extraTemplateStrings;
    return (
      <ModalOverlay {...props} button_label={button_label} title={header}>
        <div className="onboardingMessageContainer">
          {props.bundle.map(message => (
            <OnboardingCard
              key={message.id}
              sendUserActionTelemetry={props.sendUserActionTelemetry}
              onAction={props.onAction}
              UISurface={props.UISurface}
              {...message}
            />
          ))}
        </div>
      </ModalOverlay>
    );
  }
}
