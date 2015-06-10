/*
 * Copied from <https://github.com/ryanseddon/react-frame-component> 0.3.2,
 * by Ryan Seddon, under the MIT license, since that original version requires
 * a browserify-style loader.
 */

/**
 * This is an array of frames that are queued and waiting to be loaded before
 * their rendering is completed.
 *
 * @type {Array}
 */
window.queuedFrames = [];

/**
 * Renders this.props.children inside an <iframe>.
 *
 * Works by creating the iframe, waiting for that to finish, and then
 * rendering the children inside that.  Waits for a while in the hopes that the
 * contents will have been rendered, and then fires a callback indicating that.
 *
 * @see onContentsRendered for the gory details about this.
 *
 * @type {ReactComponentFactory<P>}
 */
window.Frame = React.createClass({
  propTypes: {
    style: React.PropTypes.object,
    head: React.PropTypes.node,
    width: React.PropTypes.number,
    height: React.PropTypes.number,
    onContentsRendered: React.PropTypes.func
  },
  render: function() {
    return React.createElement("iframe", {
      style: this.props.style,
      head: this.props.head,
      width: this.props.width,
      height: this.props.height
    });
  },
  componentDidMount: function() {
    this.renderFrameContents();
  },
  renderFrameContents: function() {
    var doc = this.getDOMNode().contentDocument;
    if (doc && doc.readyState === "complete") {
      // Remove this from the queue.
      window.queuedFrames.splice(window.queuedFrames.indexOf(this), 1);

      var iframeHead = doc.querySelector("head");
      var parentHeadChildren = document.querySelector("head").children;

      [].forEach.call(parentHeadChildren, function(parentHeadNode) {
        iframeHead.appendChild(parentHeadNode.cloneNode(true));
      });

      var contents = React.createElement("div",
        undefined,
        this.props.head,
        this.props.children
      );

      React.render(contents, doc.body, this.fireOnContentsRendered.bind(this));

      // Set the RTL mode. We assume for now that rtl is the only query parameter.
      //
      // See also "ShowCase" in ui-showcase.jsx
      if (document.location.search === "?rtl=1") {
        doc.documentElement.setAttribute("lang", "ar");
        doc.documentElement.setAttribute("dir", "rtl");
      }
    } else {
      // Queue it, only if it isn't already. We do need to set the timeout
      // regardless, as this function can get re-entered several times.
      if (window.queuedFrames.indexOf(this) === -1) {
        window.queuedFrames.push(this);
      }
      setTimeout(this.renderFrameContents.bind(this), 0);
    }
  },
  /**
   * Fires the onContentsRendered callback passed in via this.props,
   * with the first argument set to the window global used by the iframe.
   * This is useful in extracting things specific to that iframe (such as
   * the matchMedia function) for use by code running in that iframe.  Once
   * React gets a more complete "context" feature:
   *
   * https://facebook.github.io/react/blog/2015/02/24/streamlining-react-elements.html#solution-make-context-parent-based-instead-of-owner-based
   *
   * we should be able to avoid reaching into the DOM like this.
   *
   * XXX wait a little while.  After React has rendered this iframe (eg the
   * virtual DOM cache gets flushed to the browser), there's still more stuff
   * that needs to happen before layout completes.  If onContentsRendered fires
   * before that happens, the wrong sizes (eg remote stream vertical height
   * of 0) are used to compute the position in the MediaSetupStream, resulting
   * in everything looking wonky.  One high likelihood candidate for the delay
   * here involves loading/decode poster images, but even using link
   * rel=prefetch on those isn't enough to workaround this problem, so there
   * may be more.
   *
   * There doesn't appear to be a good cross-browser way to handle this
   * at the moment without gross violation of encapsulation (see
   * http://stackoverflow.com/questions/27241186/how-to-determine-when-document-has-loaded-after-loading-external-csshttp://stackoverflow.com/questions/27241186/how-to-determine-when-document-has-loaded-after-loading-external-css
   * for discussion of a related problem.
   *
   * For now, just wait for multiple seconds.  Yuck.
   */
  fireOnContentsRendered: function() {
    if (!this.props.onContentsRendered) {
      return;
    }

    var contentWindow;
    try {
      contentWindow = this.getDOMNode().contentWindow;
      if (!contentWindow) {
        throw new Error("no content window returned");
      }

    } catch (ex) {
      console.error("exception getting content window", ex);
    }

    // Using bind to construct a "partial function", where |this| is unchanged,
    // but the first parameter is guaranteed to be set.  Details at
    // https://developer.mozilla.org/de/docs/Web/JavaScript/Reference/Global_Objects/Function/bind#Example.3A_Partial_Functions
    setTimeout(this.props.onContentsRendered.bind(undefined, contentWindow),
               3000);
  },
  componentDidUpdate: function() {
    this.renderFrameContents();
  },
  componentWillUnmount: function() {
    React.unmountComponentAtNode(React.findDOMNode(this).contentDocument.body);
  }
});
