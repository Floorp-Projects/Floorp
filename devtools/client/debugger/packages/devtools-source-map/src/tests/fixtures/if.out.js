'use strict';

function componentWillReceiveProps(nextProps) {
  console.log('start');
  var selectedSource = nextProps.selectedSource;


  if (nextProps.startPanelSize !== this.props.startPanelSize || nextProps.endPanelSize !== this.props.endPanelSize) {
    this.state.editor.codeMirror.setSize();
  }
  console.log('done');
}

//# sourceMappingURL=if.out.js.map