import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {addLocaleData, injectIntl, IntlProvider} from "react-intl";
import {ASRouterAdmin} from "content-src/components/ASRouterAdmin/ASRouterAdmin";
import {ConfirmDialog} from "content-src/components/ConfirmDialog/ConfirmDialog";
import {connect} from "react-redux";
import {ErrorBoundary} from "content-src/components/ErrorBoundary/ErrorBoundary";
import {ManualMigration} from "content-src/components/ManualMigration/ManualMigration";
import {PrerenderData} from "common/PrerenderData.jsm";
import React from "react";
import {Search} from "content-src/components/Search/Search";
import {Sections} from "content-src/components/Sections/Sections";
import {StartupOverlay} from "content-src/components/StartupOverlay/StartupOverlay";

const PrefsButton = injectIntl(props => (
  <div className="prefs-button">
    <button className="icon icon-settings" onClick={props.onClick} title={props.intl.formatMessage({id: "settings_pane_button_label"})} />
  </div>
));

// Add the locale data for pluralization and relative-time formatting for now,
// this just uses english locale data. We can make this more sophisticated if
// more features are needed.
function addLocaleDataForReactIntl(locale) {
  addLocaleData([{locale, parentLocale: "en"}]);
}

// Returns a function will not be continuously triggered when called. The
// function will be triggered if called again after `wait` milliseconds.
function debounce(func, wait) {
  let timer;
  return (...args) => {
    if (timer) { return; }

    let wakeUp = () => { timer = null; };

    timer = setTimeout(wakeUp, wait);
    func.apply(this, args);
  };
}

export class _Base extends React.PureComponent {
  componentWillMount() {
    const {locale} = this.props;
    addLocaleDataForReactIntl(locale);
    if (this.props.isFirstrun) {
      global.document.body.classList.add("welcome", "hide-main");
    }
  }

  componentDidMount() {
    // Request state AFTER the first render to ensure we don't cause the
    // prerendered DOM to be unmounted. Otherwise, NEW_TAB_STATE_REQUEST is
    // dispatched right after the store is ready.
    if (this.props.isPrerendered) {
      this.props.dispatch(ac.AlsoToMain({type: at.NEW_TAB_STATE_REQUEST}));
      this.props.dispatch(ac.AlsoToMain({type: at.PAGE_PRERENDERED}));
    }
  }

  componentWillUnmount() {
    this.updateTheme();
  }

  componentWillUpdate() {
    this.updateTheme();
  }

  updateTheme() {
    const bodyClassName = [
      "activity-stream",
      // If we skipped the about:welcome overlay and removed the CSS classes
      // we don't want to add them back to the Activity Stream view
      document.body.classList.contains("welcome") ? "welcome" : "",
      document.body.classList.contains("hide-main") ? "hide-main" : ""
    ].filter(v => v).join(" ");
    global.document.body.className = bodyClassName;
  }

  render() {
    const {props} = this;
    const {App, locale, strings} = props;
    const {initialized} = App;

    const prefs = props.Prefs.values;
    if (prefs["asrouter.devtoolsEnabled"] && window.location.hash === "#asrouter") {
      return (<ASRouterAdmin />);
    }

    if (!props.isPrerendered && !initialized) {
      return null;
    }

    return (<IntlProvider locale={locale} messages={strings}>
        <ErrorBoundary className="base-content-fallback">
          <BaseContent {...this.props} />
        </ErrorBoundary>
      </IntlProvider>);
  }
}

export class BaseContent extends React.PureComponent {
  constructor(props) {
    super(props);
    this.openPreferences = this.openPreferences.bind(this);
    this.onWindowScroll = debounce(this.onWindowScroll.bind(this), 5);
    this.state = {fixedSearch: false};
  }

  componentDidMount() {
    global.addEventListener("scroll", this.onWindowScroll);
  }

  componentWillUnmount() {
    global.removeEventListener("scroll", this.onWindowScroll);
  }

  onWindowScroll() {
    const SCROLL_THRESHOLD = 34;
    if (global.scrollY > SCROLL_THRESHOLD && !this.state.fixedSearch) {
      this.setState({fixedSearch: true});
    } else if (global.scrollY <= SCROLL_THRESHOLD && this.state.fixedSearch) {
      this.setState({fixedSearch: false});
    }
  }

  openPreferences() {
    this.props.dispatch(ac.OnlyToMain({type: at.SETTINGS_OPEN}));
    this.props.dispatch(ac.UserEvent({event: "OPEN_NEWTAB_PREFS"}));
  }

  render() {
    const {props} = this;
    const {App} = props;
    const {initialized} = App;
    const prefs = props.Prefs.values;

    const shouldBeFixedToTop = PrerenderData.arePrefsValid(name => prefs[name]);

    const outerClassName = [
      "outer-wrapper",
      shouldBeFixedToTop && "fixed-to-top",
      prefs.showSearch && this.state.fixedSearch && "fixed-search"
    ].filter(v => v).join(" ");

    return (
      <div>
        <div className={outerClassName}>
          <main>
            {prefs.showSearch &&
              <div className="non-collapsible-section">
                <ErrorBoundary>
                  <Search />
                </ErrorBoundary>
              </div>
            }
            <div className={`body-wrapper${(initialized ? " on" : "")}`}>
              {!prefs.migrationExpired &&
                <div className="non-collapsible-section">
                  <ManualMigration />
                </div>
                }
              <Sections />
              <PrefsButton onClick={this.openPreferences} />
            </div>
            <ConfirmDialog />
          </main>
        </div>
        {this.props.isFirstrun && <StartupOverlay />}
      </div>);
  }
}

export const Base = connect(state => ({App: state.App, Prefs: state.Prefs, Sections: state.Sections}))(_Base);
