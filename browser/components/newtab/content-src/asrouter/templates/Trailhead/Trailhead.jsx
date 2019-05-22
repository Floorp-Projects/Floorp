import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {injectIntl} from "react-intl";
import {ModalOverlayWrapper} from "../../components/ModalOverlay/ModalOverlay";
import {OnboardingCard} from "../OnboardingMessage/OnboardingMessage";
import React from "react";

const FLUENT_FILES = [
  "branding/brand.ftl",
  "browser/branding/brandings.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
];

// From resource://devtools/client/shared/focus.js
const FOCUSABLE_SELECTOR = [
  "a[href]:not([tabindex='-1'])",
  "button:not([disabled]):not([tabindex='-1'])",
  "iframe:not([tabindex='-1'])",
  "input:not([disabled]):not([tabindex='-1'])",
  "select:not([disabled]):not([tabindex='-1'])",
  "textarea:not([disabled]):not([tabindex='-1'])",
  "[tabindex]:not([tabindex='-1'])",
].join(", ");

export class _Trailhead extends React.PureComponent {
  constructor(props) {
    super(props);
    this.closeModal = this.closeModal.bind(this);
    this.hideCardPanel = this.hideCardPanel.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onStartBlur = this.onStartBlur.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);
    this.onCardAction = this.onCardAction.bind(this);

    this.state = {
      emailInput: "",
      isModalOpen: true,
      showCardPanel: true,
      showCards: false,
      // The params below are for FxA metrics
      deviceId: "",
      flowId: "",
      flowBeginTime: 0,
    };
    this.fxaMetricsInitialized = false;
  }

  get dialog() {
    return this.props.document.getElementById("trailheadDialog");
  }

  async componentWillMount() {
    FLUENT_FILES.forEach(file => {
      const link = document.head.appendChild(document.createElement("link"));
      link.href = file;
      link.rel = "localization";
    });

    await this.componentWillUpdate(this.props);
  }

  // Get the fxa data if we don't have it yet from mount or update
  async componentWillUpdate(props) {
    if (props.fxaEndpoint && !this.fxaMetricsInitialized) {
      try {
        this.fxaMetricsInitialized = true;
        const url = new URL(`${props.fxaEndpoint}/metrics-flow?entrypoint=activity-stream-firstrun&form_type=email`);
        this.addUtmParams(url);
        const response = await fetch(url, {credentials: "omit"});
        if (response.status === 200) {
          const {deviceId, flowId, flowBeginTime} = await response.json();
          this.setState({deviceId, flowId, flowBeginTime});
        } else {
          props.dispatch(ac.OnlyToMain({type: at.TELEMETRY_UNDESIRED_EVENT, data: {event: "FXA_METRICS_FETCH_ERROR", value: response.status}}));
        }
      } catch (error) {
        props.dispatch(ac.OnlyToMain({type: at.TELEMETRY_UNDESIRED_EVENT, data: {event: "FXA_METRICS_ERROR"}}));
      }
    }
  }

  componentDidMount() {
    // We need to remove hide-main since we should show it underneath everything that has rendered
    this.props.document.body.classList.remove("hide-main");

    // Add inline-onboarding class to disable fixed search header and fixed positioned settings icon
    this.props.document.body.classList.add("inline-onboarding");

    // The rest of the page is "hidden" when the modal is open
    if (this.props.message.content) {
      this.props.document.getElementById("root").setAttribute("aria-hidden", "true");

      // Start with focus in the email input box
      this.dialog.querySelector("input[name=email]").focus();
    } else {
      // No modal overlay, let the user scroll and deal them some cards.
      this.props.document.body.classList.remove("welcome");

      if (this.props.message.includeBundle || this.props.message.cards) {
        this.revealCards();
      }
    }
  }

  componentWillUnmount() {
    this.props.document.body.classList.remove("inline-onboarding");
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({emailInput: e.target.value});
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onStartBlur(event) {
    // Make sure focus stays within the dialog when tabbing from the button
    const {dialog} = this;
    if (event.relatedTarget &&
        !(dialog.compareDocumentPosition(event.relatedTarget) &
          dialog.DOCUMENT_POSITION_CONTAINED_BY)) {
      dialog.querySelector(FOCUSABLE_SELECTOR).focus();
    }
  }

  onSubmit(event) {
    // Dynamically require the email on submission so screen readers don't read
    // out it's always required because there's also ways to skip the modal
    const {email} = event.target.elements;
    if (!email.value.length) {
      email.required = true;
      email.checkValidity();
      event.preventDefault();
      return;
    }

    this.props.dispatch(ac.UserEvent({event: "SUBMIT_EMAIL", ...this._getFormInfo()}));

    global.addEventListener("visibilitychange", this.closeModal);
  }

  closeModal(ev) {
    global.removeEventListener("visibilitychange", this.closeModal);
    this.props.document.body.classList.remove("welcome");
    this.props.document.getElementById("root").removeAttribute("aria-hidden");
    this.setState({isModalOpen: false});
    this.revealCards();

    // If closeModal() was triggered by a visibilitychange event, the user actually
    // submitted the email form so we don't send a SKIPPED_SIGNIN ping.
    if (!ev || ev.type !== "visibilitychange") {
      this.props.dispatch(ac.UserEvent({event: "SKIPPED_SIGNIN", ...this._getFormInfo()}));
    }

    // Bug 1190882 - Focus in a disappearing dialog confuses screen readers
    this.props.document.activeElement.blur();
  }

  /**
   * Report to telemetry additional information about the form submission.
   */
  _getFormInfo() {
    const value = {has_flow_params: this.state.flowId.length > 0};
    return {value};
  }

  onInputInvalid(e) {
    let error = e.target.previousSibling;
    error.classList.add("active");
    e.target.classList.add("invalid");
    e.preventDefault(); // Override built-in form validation popup
    e.target.focus();
  }

  hideCardPanel() {
    this.setState({showCardPanel: false});
    this.props.onDismissBundle();
  }

  revealCards() {
    this.setState({showCards: true});
  }

  getStringValue(str) {
    if (str.property_id) {
      str.value = this.props.intl.formatMessage({id: str.property_id});
    }
    return str.value;
  }

  /**
   * Takes in a url as a string or URL object and returns a URL object with the
   * utm_* parameters added to it. If a URL object is passed in, the paraemeters
   * are added to it (the return value can be ignored in that case as it's the
   * same object).
   */
  addUtmParams(url, isCard = false) {
    let returnUrl = url;
    if (typeof returnUrl === "string") {
      returnUrl = new URL(url);
    }
    returnUrl.searchParams.append("utm_source", "activity-stream");
    returnUrl.searchParams.append("utm_campaign", "firstrun");
    returnUrl.searchParams.append("utm_medium", "referral");
    returnUrl.searchParams.append("utm_term", `${this.props.message.utm_term}${isCard ? "-card" : ""}`);
    return returnUrl;
  }

  onCardAction(action) {
    let actionUpdates = {};

    if (action.type === "OPEN_URL") {
      let url = new URL(action.data.args);
      this.addUtmParams(url, true);

      if (action.addFlowParams) {
        url.searchParams.append("device_id", this.state.deviceId);
        url.searchParams.append("flow_id", this.state.flowId);
        url.searchParams.append("flow_begin_time", this.state.flowBeginTime);
      }

      actionUpdates = {data: {...action.data, args: url}};
    }

    this.props.onAction({...action, ...actionUpdates});
  }

  render() {
    const {props} = this;
    const {bundle: cards, content, utm_term} = props.message;
    const innerClassName = [
      "trailhead",
      content && content.className,
    ].filter(v => v).join(" ");
    return (<>
    {this.state.isModalOpen && content ? <ModalOverlayWrapper innerClassName={innerClassName} onClose={this.closeModal} id="trailheadDialog" headerId="trailheadHeader">
      <div className="trailheadInner">
        <div className="trailheadContent">
          <h1 data-l10n-id={content.title.string_id}
            id="trailheadHeader">{this.getStringValue(content.title)}</h1>
          {content.subtitle &&
            <p data-l10n-id={content.subtitle.string_id}>{this.getStringValue(content.subtitle)}</p>
          }
          <ul className="trailheadBenefits">
            {content.benefits.map(item => (
              <li key={item.id} className={item.id}>
                <h3 data-l10n-id={item.title.string_id}>{this.getStringValue(item.title)}</h3>
                <p data-l10n-id={item.text.string_id}>{this.getStringValue(item.text)}</p>
              </li>
            ))}
          </ul>
          <a className="trailheadLearn" data-l10n-id={content.learn.text.string_id} href={this.addUtmParams(content.learn.url)} target="_blank" rel="noopener noreferrer">
            {this.getStringValue(content.learn.text)}
          </a>
        </div>
        <div role="group" aria-labelledby="joinFormHeader" aria-describedby="joinFormBody" className="trailheadForm">
          <h3 id="joinFormHeader" data-l10n-id={content.form.title.string_id}>{this.getStringValue(content.form.title)}</h3>
          <p id="joinFormBody" data-l10n-id={content.form.text.string_id}>{this.getStringValue(content.form.text)}</p>
          <form method="get" action={this.props.fxaEndpoint} target="_blank" rel="noopener noreferrer" onSubmit={this.onSubmit}>
            <input name="service" type="hidden" value="sync" />
            <input name="action" type="hidden" value="email" />
            <input name="context" type="hidden" value="fx_desktop_v3" />
            <input name="entrypoint" type="hidden" value="activity-stream-firstrun" />
            <input name="utm_source" type="hidden" value="activity-stream" />
            <input name="utm_campaign" type="hidden" value="firstrun" />
            <input name="utm_term" type="hidden" value={utm_term} />
            <input name="device_id" type="hidden" value={this.state.deviceId} />
            <input name="flow_id" type="hidden" value={this.state.flowId} />
            <input name="flow_begin_time" type="hidden" value={this.state.flowBeginTime} />
            <input name="style" type="hidden" value="trailhead" />
            <p data-l10n-id="onboarding-join-form-email-error" className="error" />
            <input
              data-l10n-id={content.form.email.string_id}
              placeholder={this.getStringValue(content.form.email)}
              name="email"
              type="email"
              onInvalid={this.onInputInvalid}
              onChange={this.onInputChange} />
            <p className="trailheadTerms" data-l10n-id="onboarding-join-form-legal">
              <a data-l10n-name="terms" target="_blank" rel="noopener noreferrer"
                href={this.addUtmParams("https://accounts.firefox.com/legal/terms")} />
              <a data-l10n-name="privacy" target="_blank" rel="noopener noreferrer"
                href={this.addUtmParams("https://accounts.firefox.com/legal/privacy")} />
            </p>
            <button data-l10n-id={content.form.button.string_id} type="submit">
              {this.getStringValue(content.form.button)}
            </button>
          </form>
        </div>
      </div>

      <button className="trailheadStart"
        data-l10n-id={content.skipButton.string_id}
        onBlur={this.onStartBlur}
        onClick={this.closeModal}>{this.getStringValue(content.skipButton)}</button>
    </ModalOverlayWrapper> : null}
    {(cards && cards.length) ? <div className={`trailheadCards ${this.state.showCardPanel ? "expanded" : "collapsed"}`}>
      <div className="trailheadCardsInner"
        aria-hidden={!this.state.showCards}>
        <h1 data-l10n-id="onboarding-welcome-header" />
        <div className={`trailheadCardGrid${this.state.showCards ? " show" : ""}`}>
        {cards.map(card => (
          <OnboardingCard key={card.id}
            className="trailheadCard"
            sendUserActionTelemetry={props.sendUserActionTelemetry}
            onAction={this.onCardAction}
            UISurface="TRAILHEAD"
            {...card} />
        ))}
        </div>
        {this.state.showCardPanel &&
          <button
            className="icon icon-dismiss" onClick={this.hideCardPanel}
            title={props.intl.formatMessage({id: "menu_action_dismiss"})}
            aria-label={props.intl.formatMessage({id: "menu_action_dismiss"})} />
        }
      </div>
    </div> : null}
    </>);
  }
}

export const Trailhead = injectIntl(_Trailhead);
