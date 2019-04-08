function componentWillReceiveProps(nextProps) {
	console.log('start');
    const { selectedSource } = nextProps;

    if (
      nextProps.startPanelSize !== this.props.startPanelSize ||
      nextProps.endPanelSize !== this.props.endPanelSize
    ) {
      this.state.editor.codeMirror.setSize();
    }
	console.log('done');
}
