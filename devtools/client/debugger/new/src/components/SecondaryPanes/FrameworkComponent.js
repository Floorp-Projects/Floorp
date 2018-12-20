/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "react-redux";
import actions from "../../actions";

import { createObjectClient } from "../../client/firefox";
import { getSelectedFrame, getAllPopupObjectProperties } from "../../selectors";

import { objectInspector } from "devtools-reps";
import { isReactComponent } from "../../utils/preview";

import type { Frame, Grip } from "../../types";

const {
  component: ObjectInspector,
  utils: {
    createNode,
    getChildren,
    loadProperties: { loadItemProperties }
  }
} = objectInspector;

type Props = {
  selectedFrame: Frame,
  popupObjectProperties: Object,
  setPopupObjectProperties: typeof actions.setPopupObjectProperties,
  openElementInInspector: (grip: Grip) => void
};

class FrameworkComponent extends PureComponent<Props> {
  async componentWillMount() {
    const expression = "this;";
    const { selectedFrame, setPopupObjectProperties } = this.props;
    const value = selectedFrame.this;

    const root = createNode({ name: expression, contents: { value } });
    const properties = await loadItemProperties(root, createObjectClient);
    if (properties) {
      setPopupObjectProperties(value, properties);
    }
  }

  renderReactComponent() {
    const {
      selectedFrame,
      popupObjectProperties,
      openElementInInspector
    } = this.props;
    const expression = "this;";
    const value = selectedFrame.this;
    const root = {
      name: expression,
      path: expression,
      contents: { value }
    };

    const loadedRootProperties = popupObjectProperties[value.actor];
    if (!loadedRootProperties) {
      return null;
    }

    let roots = getChildren({
      item: root,
      loadedProperties: new Map([[root.path, loadedRootProperties]])
    });

    roots = roots.filter(r => ["state", "props"].includes(r.name));

    return (
      <div className="pane framework-component">
        <ObjectInspector
          roots={roots}
          autoExpandAll={false}
          autoExpandDepth={0}
          disableWrap={true}
          focusable={false}
          dimTopLevelWindow={true}
          createObjectClient={grip => createObjectClient(grip)}
          onDOMNodeClick={grip => openElementInInspector(grip)}
          onInspectIconClick={grip => openElementInInspector(grip)}
        />
      </div>
    );
  }

  render() {
    const { selectedFrame } = this.props;
    if (selectedFrame && isReactComponent(selectedFrame.this)) {
      return this.renderReactComponent();
    }

    return null;
  }
}

const mapStateToProps = state => ({
  selectedFrame: getSelectedFrame(state),
  popupObjectProperties: getAllPopupObjectProperties(state),
  openElementInInspector: actions.openElementInInspectorCommand
});

export default connect(
  mapStateToProps,
  {
    setPopupObjectProperties: actions.setPopupObjectProperties
  }
)(FrameworkComponent);
