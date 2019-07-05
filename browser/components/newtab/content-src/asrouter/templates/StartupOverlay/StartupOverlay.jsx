import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { connect } from "react-redux";
import React from "react";

const FLUENT_FILES = [
  "branding/brand.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
];

export class _StartupOverlay extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onInputChange = this.onInputChange.bind(this);
    this.onSubmit = this.onSubmit.bind(this);
    this.clickSkip = this.clickSkip.bind(this);
    this.initScene = this.initScene.bind(this);
    this.removeOverlay = this.removeOverlay.bind(this);
    this.onInputInvalid = this.onInputInvalid.bind(this);

    this.utmParams =
      "utm_source=activity-stream&utm_campaign=firstrun&utm_medium=referral&utm_term=trailhead-control";

    this.state = {
      emailInput: "",
      overlayRemoved: false,
      deviceId: "",
      flowId: "",
      flowBeginTime: 0,
    };
    this.didFetch = false;
  }

  async componentWillUpdate() {
    if (this.props.fxa_endpoint && !this.didFetch) {
      try {
        this.didFetch = true;
        const fxaParams = "entrypoint=activity-stream-firstrun&form_type=email";
        const response = await fetch(
          `${this.props.fxa_endpoint}/metrics-flow?${fxaParams}&${
            this.utmParams
          }`,
          { credentials: "omit" }
        );
        if (response.status === 200) {
          const { deviceId, flowId, flowBeginTime } = await response.json();
          this.setState({ deviceId, flowId, flowBeginTime });
        } else {
          this.props.dispatch(
            ac.OnlyToMain({
              type: at.TELEMETRY_UNDESIRED_EVENT,
              data: {
                event: "FXA_METRICS_FETCH_ERROR",
                value: response.status,
              },
            })
          );
        }
      } catch (error) {
        this.props.dispatch(
          ac.OnlyToMain({
            type: at.TELEMETRY_UNDESIRED_EVENT,
            data: { event: "FXA_METRICS_ERROR" },
          })
        );
      }
    }
  }

  async componentWillMount() {
    FLUENT_FILES.forEach(file => {
      const link = document.head.appendChild(document.createElement("link"));
      link.href = file;
      link.rel = "localization";
    });

    await this.componentWillUpdate(this.props);
  }

  componentDidMount() {
    this.initScene();
  }

  initScene() {
    // Timeout to allow the scene to render once before attaching the attribute
    // to trigger the animation.
    setTimeout(() => {
      this.setState({ show: true });
      this.props.onReady();
    }, 10);
  }

  removeOverlay() {
    window.removeEventListener("visibilitychange", this.removeOverlay);
    document.body.classList.remove("hide-main", "fxa");
    this.setState({ show: false });
    this.props.onBlock();
    setTimeout(() => {
      // Allow scrolling and fully remove overlay after animation finishes.
      document.body.classList.remove("welcome");
      this.setState({ overlayRemoved: true });
    }, 400);
  }

  onInputChange(e) {
    let error = e.target.previousSibling;
    this.setState({ emailInput: e.target.value });
    error.classList.remove("active");
    e.target.classList.remove("invalid");
  }

  onSubmit() {
    this.props.dispatch(
      ac.UserEvent({ event: "SUBMIT_EMAIL", ...this._getFormInfo() })
    );

    window.addEventListener("visibilitychange", this.removeOverlay);
  }

  clickSkip() {
    this.props.dispatch(
      ac.UserEvent({ event: "SKIPPED_SIGNIN", ...this._getFormInfo() })
    );
    this.removeOverlay();
  }

  /**
   * Report to telemetry additional information about the form submission.
   */
  _getFormInfo() {
    const value = { has_flow_params: this.state.flowId.length > 0 };
    return { value };
  }

  onInputInvalid(e) {
    let error = e.target.previousSibling;
    error.classList.add("active");
    e.target.classList.add("invalid");
    e.preventDefault(); // Override built-in form validation popup
    e.target.focus();
  }

  render() {
    // When skipping the onboarding tour we show AS but we are still on
    // about:welcome, prop.isFirstrun is true and StartupOverlay is rendered
    if (this.state.overlayRemoved) {
      return null;
    }

    return (
      <div className={`overlay-wrapper ${this.state.show ? "show" : ""}`}>
        <div className="background" />
        <div className="firstrun-scene">
          <div className="fxaccounts-container">
            <div className="firstrun-left-divider">
              <h1
                className="firstrun-title"
                data-l10n-id="onboarding-sync-welcome-header"
              />
              <p
                className="firstrun-content"
                data-l10n-id="onboarding-sync-welcome-content"
              />
              <a
                className="firstrun-link"
                href={`https://www.mozilla.org/firefox/features/sync/?${
                  this.utmParams
                }`}
                target="_blank"
                rel="noopener noreferrer"
                data-l10n-id="onboarding-sync-welcome-learn-more-link"
              />
            </div>
            <div className="firstrun-sign-in">
              <p className="form-header">
                <span data-l10n-id="onboarding-sync-form-header" />
                <span
                  className="sub-header"
                  data-l10n-id="onboarding-sync-form-sub-header"
                />
              </p>
              <form
                method="get"
                action={this.props.fxa_endpoint}
                target="_blank"
                rel="noopener noreferrer"
                onSubmit={this.onSubmit}
              >
                <input name="service" type="hidden" value="sync" />
                <input name="action" type="hidden" value="email" />
                <input name="context" type="hidden" value="fx_desktop_v3" />
                <input
                  name="entrypoint"
                  type="hidden"
                  value="activity-stream-firstrun"
                />
                <input
                  name="utm_source"
                  type="hidden"
                  value="activity-stream"
                />
                <input name="utm_campaign" type="hidden" value="firstrun" />
                <input name="utm_medium" type="hidden" value="referral" />
                <input
                  name="utm_term"
                  type="hidden"
                  value="trailhead-control"
                />
                <input
                  name="device_id"
                  type="hidden"
                  value={this.state.deviceId}
                />
                <input name="flow_id" type="hidden" value={this.state.flowId} />
                <input
                  name="flow_begin_time"
                  type="hidden"
                  value={this.state.flowBeginTime}
                />
                <span
                  className="error"
                  data-l10n-id="onboarding-sync-form-invalid-input"
                />
                <input
                  className="email-input"
                  name="email"
                  type="email"
                  required="true"
                  onInvalid={this.onInputInvalid}
                  onChange={this.onInputChange}
                  data-l10n-id="onboarding-sync-form-input"
                />
                <div className="extra-links">
                  <p data-l10n-id="onboarding-sync-legal-notice">
                    <a
                      data-l10n-name="terms"
                      target="_blank"
                      rel="noopener noreferrer"
                      href={`${this.props.fxa_endpoint}/legal/terms?${
                        this.utmParams
                      }`}
                    />
                    <a
                      data-l10n-name="privacy"
                      target="_blank"
                      rel="noopener noreferrer"
                      href={`${this.props.fxa_endpoint}/legal/privacy?${
                        this.utmParams
                      }`}
                    />
                  </p>
                </div>
                <button
                  className="continue-button"
                  type="submit"
                  data-l10n-id="onboarding-sync-form-continue-button"
                />
              </form>
              <button
                className="skip-button"
                disabled={!!this.state.emailInput}
                onClick={this.clickSkip}
                data-l10n-id="onboarding-sync-form-skip-login-button"
              />
            </div>
          </div>
        </div>
      </div>
    );
  }
}

const getState = state => ({ fxa_endpoint: state.Prefs.values.fxa_endpoint });
export const StartupOverlay = connect(getState)(_StartupOverlay);
