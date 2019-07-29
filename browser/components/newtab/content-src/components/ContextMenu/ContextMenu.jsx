import React from "react";

export class ContextMenu extends React.PureComponent {
  constructor(props) {
    super(props);
    this.hideContext = this.hideContext.bind(this);
    this.onShow = this.onShow.bind(this);
    this.onClick = this.onClick.bind(this);
  }

  hideContext() {
    this.props.onUpdate(false);
  }

  onShow() {
    if (this.props.onShow) {
      this.props.onShow();
    }
  }

  componentDidMount() {
    this.onShow();
    setTimeout(() => {
      global.addEventListener("click", this.hideContext);
    }, 0);
  }

  componentWillUnmount() {
    global.removeEventListener("click", this.hideContext);
  }

  onClick(event) {
    // Eat all clicks on the context menu so they don't bubble up to window.
    // This prevents the context menu from closing when clicking disabled items
    // or the separators.
    event.stopPropagation();
  }

  render() {
    // Disabling focus on the menu span allows the first tab to focus on the first menu item instead of the wrapper.
    return (
      // eslint-disable-next-line jsx-a11y/interactive-supports-focus
      <span
        role="menu"
        className="context-menu"
        onClick={this.onClick}
        onKeyDown={this.onClick}
      >
        <ul className="context-menu-list">
          {this.props.options.map((option, i) =>
            option.type === "separator" ? (
              <li key={i} className="separator" />
            ) : (
              option.type !== "empty" && (
                <ContextMenuItem
                  key={i}
                  option={option}
                  hideContext={this.hideContext}
                  tabIndex="0"
                />
              )
            )
          )}
        </ul>
      </span>
    );
  }
}

export class ContextMenuItem extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  onClick() {
    this.props.hideContext();
    this.props.option.onClick();
  }

  // This selects the correct node based on the key pressed
  focusSibling(target, key) {
    const parent = target.parentNode;
    const closestSiblingSelector =
      key === "ArrowUp" ? "previousSibling" : "nextSibling";
    if (!parent[closestSiblingSelector]) {
      return;
    }
    if (parent[closestSiblingSelector].firstElementChild) {
      parent[closestSiblingSelector].firstElementChild.focus();
    } else {
      parent[closestSiblingSelector][
        closestSiblingSelector
      ].firstElementChild.focus();
    }
  }

  onKeyDown(event) {
    const { option } = this.props;
    switch (event.key) {
      case "Tab":
        // tab goes down in context menu, shift + tab goes up in context menu
        // if we're on the last item, one more tab will close the context menu
        // similarly, if we're on the first item, one more shift + tab will close it
        if (
          (event.shiftKey && option.first) ||
          (!event.shiftKey && option.last)
        ) {
          this.props.hideContext();
        }
        break;
      case "ArrowUp":
      case "ArrowDown":
        event.preventDefault();
        this.focusSibling(event.target, event.key);
        break;
      case "Enter":
        this.props.hideContext();
        option.onClick();
        break;
      case "Escape":
        this.props.hideContext();
        break;
    }
  }

  render() {
    const { option } = this.props;
    return (
      <li role="menuitem" className="context-menu-item">
        <button
          className={option.disabled ? "disabled" : ""}
          tabIndex="0"
          onClick={this.onClick}
          onKeyDown={this.onKeyDown}
        >
          {option.icon && (
            <span className={`icon icon-spacer icon-${option.icon}`} />
          )}
          <span data-l10n-id={option.string_id || option.id} />
        </button>
      </li>
    );
  }
}
