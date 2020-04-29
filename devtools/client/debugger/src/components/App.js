/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";

import { connect } from "../utils/connect";
import { prefs, features } from "../utils/prefs";
import actions from "../actions";
import A11yIntention from "./A11yIntention";
import { ShortcutsModal } from "./ShortcutsModal";

import {
  getSelectedSource,
  getPaneCollapse,
  getActiveSearch,
  getQuickOpenEnabled,
  getOrientation,
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
  "(min-width: 10px) and (max-width: 799px)"
);

import "./variables.css";
import "./App.css";

// $FlowIgnore
import "devtools-launchpad/src/components/Root.css";

import type { ActiveSearchType } from "../selectors";

import "./shared/menu.css";

import SplitBox from "devtools-splitter";
import ProjectSearch from "./ProjectSearch";
import PrimaryPanes from "./PrimaryPanes";
import Editor from "./Editor";
import SecondaryPanes from "./SecondaryPanes";
import WelcomeBox from "./WelcomeBox";
import EditorTabs from "./Editor/Tabs";
import EditorFooter from "./Editor/Footer";
import QuickOpenModal from "./QuickOpenModal";
import WhyPaused from "./SecondaryPanes/WhyPaused";

type OwnProps = {|
  toolboxDoc: Object,
|};
type Props = {
  selectedSource: ?Source,
  orientation: OrientationType,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
  activeSearch: ?ActiveSearchType,
  quickOpenEnabled: boolean,
  toolboxDoc: Object,
  setActiveSearch: typeof actions.setActiveSearch,
  closeActiveSearch: typeof actions.closeActiveSearch,
  closeProjectSearch: typeof actions.closeProjectSearch,
  openQuickOpen: typeof actions.openQuickOpen,
  closeQuickOpen: typeof actions.closeQuickOpen,
  setOrientation: typeof actions.setOrientation,
};

type State = {
  shortcutsModalEnabled: boolean,
  startPanelSize: number,
  endPanelSize: number,
};

class App extends Component<Props, State> {
  onLayoutChange: Function;
  getChildContext: Function;
  renderEditorPane: Function;
  renderLayout: Function;
  toggleQuickOpenModal: Function;
  onEscape: Function;
  onCommandSlash: Function;

  constructor(props: Props) {
    super(props);
    this.state = {
      shortcutsModalEnabled: false,
      startPanelSize: 0,
      endPanelSize: 0,
    };
  }

  getChildContext() {
    return {
      toolboxDoc: this.props.toolboxDoc,
      shortcuts,
      l10n: L10N,
    };
  }

  componentDidMount() {
    horizontalLayoutBreakpoint.addListener(this.onLayoutChange);
    verticalLayoutBreakpoint.addListener(this.onLayoutChange);
    this.setOrientation();

    shortcuts.on(L10N.getStr("symbolSearch.search.key2"), (_, e) =>
      this.toggleQuickOpenModal(_, e, "@")
    );

    const searchKeys = [
      L10N.getStr("sources.search.key2"),
      L10N.getStr("sources.search.alt.key"),
    ];
    searchKeys.forEach(key => shortcuts.on(key, this.toggleQuickOpenModal));

    shortcuts.on(L10N.getStr("gotoLineModal.key3"), (_, e) =>
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
      L10N.getStr("sources.search.alt.key"),
    ];
    searchKeys.forEach(key => shortcuts.off(key, this.toggleQuickOpenModal));

    shortcuts.off(L10N.getStr("gotoLineModal.key3"), this.toggleQuickOpenModal);

    shortcuts.off("Escape", this.onEscape);
  }

  onEscape = (_: mixed, e: KeyboardEvent) => {
    const {
      activeSearch,
      closeActiveSearch,
      closeQuickOpen,
      quickOpenEnabled,
    } = this.props;
    const { shortcutsModalEnabled } = this.state;

    if (activeSearch) {
      e.preventDefault();
      closeActiveSearch();
    }

    if (quickOpenEnabled) {
      e.preventDefault();
      closeQuickOpen();
    }

    if (shortcutsModalEnabled) {
      e.preventDefault();
      this.toggleShortcutsModal();
    }
  };

  onCommandSlash = () => {
    this.toggleShortcutsModal();
  };

  isHorizontal() {
    return this.props.orientation === "horizontal";
  }

  toggleQuickOpenModal = (
    _: mixed,
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
          />
          <Editor startPanelSize={startPanelSize} endPanelSize={endPanelSize} />
          {this.props.endPanelCollapsed ? <WhyPaused /> : null}
          {!this.props.selectedSource ? (
            <WelcomeBox
              horizontal={horizontal}
              toggleShortcutsModal={() => this.toggleShortcutsModal()}
            />
          ) : null}
          <EditorFooter horizontal={horizontal} />
          <ProjectSearch />
        </div>
      </div>
    );
  };

  toggleShortcutsModal() {
    this.setState(prevState => ({
      shortcutsModalEnabled: !prevState.shortcutsModalEnabled,
    }));
  }

  // Important so that the tabs chevron updates appropriately when
  // the user resizes the left or right columns
  triggerEditorPaneResize() {
    const editorPane = window.document.querySelector(".editor-pane");
    if (editorPane) {
      editorPane.dispatchEvent(new Event("resizeend"));
    }
  }

  renderLayout = () => {
    const { startPanelCollapsed, endPanelCollapsed } = this.props;
    const horizontal = this.isHorizontal();

    return (
      <SplitBox
        style={{ width: "100vw" }}
        initialSize={prefs.endPanelSize}
        minSize={30}
        maxSize="70%"
        splitterSize={1}
        vert={horizontal}
        onResizeEnd={num => {
          prefs.endPanelSize = num;
          this.triggerEditorPaneResize();
        }}
        startPanel={
          <SplitBox
            style={{ width: "100vw" }}
            initialSize={prefs.startPanelSize}
            minSize={30}
            maxSize="85%"
            splitterSize={1}
            onResizeEnd={num => {
              prefs.startPanelSize = num;
            }}
            startPanelCollapsed={startPanelCollapsed}
            startPanel={<PrimaryPanes horizontal={horizontal} />}
            endPanel={this.renderEditorPane()}
          />
        }
        endPanelControl={true}
        endPanel={<SecondaryPanes horizontal={horizontal} />}
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
      <div className={classnames("debugger")}>
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
  toolboxDoc: PropTypes.object,
  shortcuts: PropTypes.object,
  l10n: PropTypes.object,
};

const mapStateToProps = state => ({
  selectedSource: getSelectedSource(state),
  startPanelCollapsed: getPaneCollapse(state, "start"),
  endPanelCollapsed: getPaneCollapse(state, "end"),
  activeSearch: getActiveSearch(state),
  quickOpenEnabled: getQuickOpenEnabled(state),
  orientation: getOrientation(state),
});

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps, {
  setActiveSearch: actions.setActiveSearch,
  closeActiveSearch: actions.closeActiveSearch,
  closeProjectSearch: actions.closeProjectSearch,
  openQuickOpen: actions.openQuickOpen,
  closeQuickOpen: actions.closeQuickOpen,
  setOrientation: actions.setOrientation,
})(App);
