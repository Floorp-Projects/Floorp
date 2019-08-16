/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { Component } from "react";
import { connect } from "../../utils/connect";
import { showMenu } from "devtools-contextmenu";

import { getSourceLocationFromMouseEvent } from "../../utils/editor";
import {
  getPrettySource,
  getIsPaused,
  getCurrentThread,
  getThreadContext,
} from "../../selectors";

import { editorMenuItems, editorItemActions } from "./menus/editor";

import type { SourceWithContent, ThreadContext } from "../../types";
import type { EditorItemActions } from "./menus/editor";
import type SourceEditor from "../../utils/editor/source-editor";

type Props = {
  cx: ThreadContext,
  contextMenu: ?MouseEvent,
  editorActions: EditorItemActions,
  clearContextMenu: () => void,
  editor: SourceEditor,
  hasPrettySource: boolean,
  isPaused: boolean,
  selectedSource: SourceWithContent,
};

class EditorMenu extends Component<Props> {
  props: Props;

  componentWillUpdate(nextProps: Props) {
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
      hasPrettySource,
      isPaused,
      contextMenu: event,
    } = props;

    const location = getSourceLocationFromMouseEvent(
      editor,
      selectedSource,
      // Use a coercion, as contextMenu is optional
      (event: any)
    );

    showMenu(
      event,
      editorMenuItems({
        cx,
        editorActions,
        selectedSource,
        hasPrettySource,
        location,
        isPaused,
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
  isPaused: getIsPaused(state, getCurrentThread(state)),
  hasPrettySource: !!getPrettySource(state, props.selectedSource.id),
});

const mapDispatchToProps = dispatch => ({
  editorActions: editorItemActions(dispatch),
});

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(EditorMenu);
