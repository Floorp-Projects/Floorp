/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { connect } from "../../utils/connect";
import { showMenu } from "../../context-menu/menu";

import { getSourceLocationFromMouseEvent } from "../../utils/editor";
import { isPretty } from "../../utils/source";
import {
  getPrettySource,
  getIsCurrentThreadPaused,
  getThreadContext,
  isSourceWithMap,
} from "../../selectors";

import { editorMenuItems, editorItemActions } from "./menus/editor";

class EditorMenu extends Component {
  componentWillUpdate(nextProps) {
    this.props.clearContextMenu();
    if (nextProps.contextMenu) {
      this.showMenu(nextProps);
    }
  }

  showMenu(props) {
    const {
      cx,
      editor,
      selectedSource,
      editorActions,
      hasMappedLocation,
      isPaused,
      editorWrappingEnabled,
      contextMenu: event,
    } = props;

    const location = getSourceLocationFromMouseEvent(
      editor,
      selectedSource,
      // Use a coercion, as contextMenu is optional
      event
    );

    showMenu(
      event,
      editorMenuItems({
        cx,
        editorActions,
        selectedSource,
        hasMappedLocation,
        location,
        isPaused,
        editorWrappingEnabled,
        selectionText: editor.codeMirror.getSelection().trim(),
        isTextSelected: editor.codeMirror.somethingSelected(),
      })
    );
  }

  render() {
    return null;
  }
}

const mapStateToProps = (state, props) => ({
  cx: getThreadContext(state),
  isPaused: getIsCurrentThreadPaused(state),
  hasMappedLocation:
    (props.selectedSource.isOriginal ||
      isSourceWithMap(state, props.selectedSource.id) ||
      isPretty(props.selectedSource)) &&
    !getPrettySource(state, props.selectedSource.id),
});

const mapDispatchToProps = dispatch => ({
  editorActions: editorItemActions(dispatch),
});

export default connect(mapStateToProps, mapDispatchToProps)(EditorMenu);
