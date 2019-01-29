/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { connect } from "../../utils/connect";
import { showMenu } from "devtools-contextmenu";

import { getSourceLocationFromMouseEvent } from "../../utils/editor";
import { getPrettySource, getIsPaused } from "../../selectors";

import { editorMenuItems, editorItemActions } from "./menus/editor";
import type { EditorMenuActions } from "./menus/editor";

type Props = {
  contextMenu: ?MouseEvent,
  editorActions: EditorMenuActions,
  clearContextMenu: () => {}
};

class EditorMenu extends Component {
  props: Props;

  componentWillUpdate(nextProps) {
    this.props.clearContextMenu();
    if (nextProps.contextMenu) {
      this.showMenu(nextProps);
    }
  }

  showMenu(props) {
    const {
      editor,
      selectedSource,
      editorActions,
      hasPrettySource,
      isPaused,
      contextMenu: event
    } = props;

    const location = getSourceLocationFromMouseEvent(
      editor,
      selectedSource,
      event
    );

    showMenu(
      event,
      editorMenuItems({
        editorActions,
        selectedSource,
        hasPrettySource,
        location,
        isPaused,
        selectionText: editor.codeMirror.getSelection().trim(),
        isTextSelected: editor.codeMirror.somethingSelected()
      })
    );
  }

  render() {
    return null;
  }
}

const mapStateToProps = (state, props) => ({
  isPaused: getIsPaused(state),
  hasPrettySource: !!getPrettySource(state, props.selectedSource.id)
});

const mapDispatchToProps = dispatch => ({
  editorActions: editorItemActions(dispatch)
});

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(EditorMenu);
