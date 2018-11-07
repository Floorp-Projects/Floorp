/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import { connect } from "react-redux";
import PropTypes from "prop-types";

import { features } from "../utils/prefs";
import actions from "../actions";
import A11yIntention from "./A11yIntention";
import { ShortcutsModal } from "./ShortcutsModal";

import {
  getSelectedSource,
  getPaneCollapse,
  getActiveSearch,
  getQuickOpenEnabled,
  getOrientation
} from "../selectors";

import type { OrientationType } from "../reducers/types";
import type { Source } from "../types";

import { KeyShortcuts } from "devtools-modules";
import Services from "devtools-services";
const shortcuts = new KeyShortcuts({ window });

const { appinfo } = Services;

const isMacOS = appinfo.OS === "Darwin";

const horizontalLayoutBreakpoint = window.matchMedia("(min-width: 800px)");
const verticalLayoutBreakpoint = window.matchMedia(
  "(min-width: 10px) and (max-width: 800px)"
);

import "./variables.css";
import "./App.css";

// $FlowIgnore
import "devtools-launchpad/src/components/Root.css";

import "./shared/menu.css";
import "./shared/reps.css";

import SplitBox from "devtools-splitter";
import ProjectSearch from "./ProjectSearch";
import PrimaryPanes from "./PrimaryPanes";
import Editor from "./Editor";
import SecondaryPanes from "./SecondaryPanes";
import WelcomeBox from "./WelcomeBox";
import EditorTabs from "./Editor/Tabs";
import QuickOpenModal from "./QuickOpenModal";

type Props = {
  selectedSource: Source,
  orientation: OrientationType,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
  activeSearch: string,
  quickOpenEnabled: boolean,
  setActiveSearch: string => void,
  closeActiveSearch: () => void,
  closeProjectSearch: () => void,
  openQuickOpen: (query?: string) => void,
  closeQuickOpen: () => void,
  setOrientation: OrientationType => void
};

type State = {
  shortcutsModalEnabled: boolean,
  startPanelSize: number,
  endPanelSize: number
};

class App extends Component<Props, State> {
  onLayoutChange: Function;
  getChildContext: Function;
  renderEditorPane: Function;
  renderLayout: Function;
  toggleQuickOpenModal: Function;
  onEscape: Function;
  onCommandSlash: Function;

  constructor(props) {
    super(props);
    this.state = {
      shortcutsModalEnabled: false,
      startPanelSize: 0,
      endPanelSize: 0
    };
  }

  getChildContext = () => {
    return { shortcuts, l10n: L10N };
  };

  componentDidMount() {
    horizontalLayoutBreakpoint.addListener(this.onLayoutChange);
    verticalLayoutBreakpoint.addListener(this.onLayoutChange);
    this.setOrientation();

    shortcuts.on(L10N.getStr("symbolSearch.search.key2"), (_, e) =>
      this.toggleQuickOpenModal(_, e, "@")
    );

    const searchKeys = [
      L10N.getStr("sources.search.key2"),
      L10N.getStr("sources.search.alt.key")
    ];
    searchKeys.forEach(key => shortcuts.on(key, this.toggleQuickOpenModal));

    shortcuts.on(L10N.getStr("gotoLineModal.key2"), (_, e) =>
      this.toggleQuickOpenModal(_, e, ":")
    );

    shortcuts.on("Escape", this.onEscape);
    shortcuts.on("Cmd+/", this.onCommandSlash);
  }

  componentWillUnmount() {
    horizontalLayoutBreakpoint.removeListener(this.onLayoutChange);
    verticalLayoutBreakpoint.removeListener(this.onLayoutChange);
    shortcuts.off(
      L10N.getStr("symbolSearch.search.key2"),
      this.toggleQuickOpenModal
    );

    const searchKeys = [
      L10N.getStr("sources.search.key2"),
      L10N.getStr("sources.search.alt.key")
    ];
    searchKeys.forEach(key => shortcuts.off(key, this.toggleQuickOpenModal));

    shortcuts.off(L10N.getStr("gotoLineModal.key2"), this.toggleQuickOpenModal);

    shortcuts.off("Escape", this.onEscape);
  }

  onEscape = (_, e) => {
    const {
      activeSearch,
      closeActiveSearch,
      closeQuickOpen,
      quickOpenEnabled
    } = this.props;

    if (activeSearch) {
      e.preventDefault();
      closeActiveSearch();
    }

    if (quickOpenEnabled) {
      e.preventDefault();
      closeQuickOpen();
    }
  };

  onCommandSlash = () => {
    this.toggleShortcutsModal();
  };

  isHorizontal() {
    return this.props.orientation === "horizontal";
  }

  toggleQuickOpenModal = (
    _,
    e: SyntheticEvent<HTMLElement>,
    query?: string
  ) => {
    const { quickOpenEnabled, openQuickOpen, closeQuickOpen } = this.props;

    e.preventDefault();
    e.stopPropagation();

    if (quickOpenEnabled === true) {
      closeQuickOpen();
      return;
    }

    if (query != null) {
      openQuickOpen(query);
      return;
    }
    openQuickOpen();
    return;
  };

  onLayoutChange = () => {
    this.setOrientation();
  };

  setOrientation() {
    // If the orientation does not match (if it is not visible) it will
    // not setOrientation, or if it is the same as before, calling
    // setOrientation will not cause a rerender.
    if (horizontalLayoutBreakpoint.matches) {
      this.props.setOrientation("horizontal");
    } else if (verticalLayoutBreakpoint.matches) {
      this.props.setOrientation("vertical");
    }
  }

  renderEditorPane = () => {
    const { startPanelCollapsed, endPanelCollapsed } = this.props;
    const { endPanelSize, startPanelSize } = this.state;
    const horizontal = this.isHorizontal();

    return (
      <div className="editor-pane">
        <div className="editor-container">
          <EditorTabs
            startPanelCollapsed={startPanelCollapsed}
            endPanelCollapsed={endPanelCollapsed}
            horizontal={horizontal}
            startPanelSize={startPanelSize}
            endPanelSize={endPanelSize}
          />
          <Editor
            horizontal={horizontal}
            startPanelSize={startPanelSize}
            endPanelSize={endPanelSize}
          />
          {!this.props.selectedSource ? (
            <WelcomeBox
              horizontal={horizontal}
              toggleShortcutsModal={() => this.toggleShortcutsModal()}
            />
          ) : null}
          <ProjectSearch />
        </div>
      </div>
    );
  };

  toggleShortcutsModal() {
    this.setState(prevState => ({
      shortcutsModalEnabled: !prevState.shortcutsModalEnabled
    }));
  }

  renderLayout = () => {
    const { startPanelCollapsed, endPanelCollapsed } = this.props;
    const horizontal = this.isHorizontal();

    const maxSize = horizontal ? "70%" : "95%";
    const primaryInitialSize = horizontal ? "250px" : "150px";

    return (
      <SplitBox
        style={{ width: "100vw" }}
        initialHeight={400}
        initialWidth={300}
        minSize={30}
        maxSize={maxSize}
        splitterSize={1}
        vert={horizontal}
        startPanel={
          <SplitBox
            style={{ width: "100vw" }}
            initialSize={primaryInitialSize}
            minSize={30}
            maxSize="85%"
            splitterSize={1}
            startPanelCollapsed={startPanelCollapsed}
            startPanel={<PrimaryPanes horizontal={horizontal} />}
            endPanel={this.renderEditorPane()}
          />
        }
        endPanelControl={true}
        endPanel={
          <SecondaryPanes
            horizontal={horizontal}
            toggleShortcutsModal={() => this.toggleShortcutsModal()}
          />
        }
        endPanelCollapsed={endPanelCollapsed}
      />
    );
  };

  renderShortcutsModal() {
    const additionalClass = isMacOS ? "mac" : "";

    if (!features.shortcuts) {
      return;
    }

    return (
      <ShortcutsModal
        additionalClass={additionalClass}
        enabled={this.state.shortcutsModalEnabled}
        handleClose={() => this.toggleShortcutsModal()}
      />
    );
  }

  render() {
    const { quickOpenEnabled } = this.props;
    return (
      <div className="debugger">
        <A11yIntention>
          {this.renderLayout()}
          {quickOpenEnabled === true && (
            <QuickOpenModal
              shortcutsModalEnabled={this.state.shortcutsModalEnabled}
              toggleShortcutsModal={() => this.toggleShortcutsModal()}
            />
          )}
          {this.renderShortcutsModal()}
        </A11yIntention>
      </div>
    );
  }
}

App.childContextTypes = {
  shortcuts: PropTypes.object,
  l10n: PropTypes.object
};

const mapStateToProps = state => ({
  selectedSource: getSelectedSource(state),
  startPanelCollapsed: getPaneCollapse(state, "start"),
  endPanelCollapsed: getPaneCollapse(state, "end"),
  activeSearch: getActiveSearch(state),
  quickOpenEnabled: getQuickOpenEnabled(state),
  orientation: getOrientation(state)
});

export default connect(
  mapStateToProps,
  {
    setActiveSearch: actions.setActiveSearch,
    closeActiveSearch: actions.closeActiveSearch,
    closeProjectSearch: actions.closeProjectSearch,
    openQuickOpen: actions.openQuickOpen,
    closeQuickOpen: actions.closeQuickOpen,
    setOrientation: actions.setOrientation
  }
)(App);
