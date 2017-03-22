# High-Level Inspector Architecture

## UI structure
The Inspector panel is a tab in the toolbox. Like all tabs, it's in its own iframe.

The high-level hierarchy looks something like this:

     Toolbox
        |
    InspectorPanel
        |
    +-------------+------------------+---------------+
    |             |                  |               |
    MarkupView  SelectorSearch  HTMLBreadcrumbs  ToolSidebar widget (iframes)
                                                     |
                                                     +- RuleView
                                                     |
                                                     +- ComputedView
                                                     |
                                                     +- LayoutView
                                                     |
                                                     +- FontInspector
                                                     |
                                                     +- AnimationInspector

## Server dependencies
- The inspector panel relies on a series of actors that live on the server.
- Some of the most important actors are actually instantiated by the toolbox, because these actors are needed for other panels to preview and link to DOM nodes. For example, the webconsole needs to output DOM nodes, highlight them in the page on mouseover, and open them in the inspector on click. This is achieved using some of the same actors that the inspector panel uses.
- See Toolbox.prototype.initInspector: This method instantiates the InspectorActor, WalkerActor and HighlighterActor lazily, whenever they're needed by a panel.

## Panel loading overview
- As with other panels, this starts with Toolbox.prototype.loadTool(id)
- For the inspector though, this calls Toolbox.prototype.initInspector
- When the panel's open method is called:
  - It uses the WalkerActor for the first time to know the default selected node (which could be a node that was selected before on the same page).
  - It starts listening to the WalkerActor's "new-root" events to know when to display a new DOM tree (when there's a page navigation).
  - It creates the breadcrumbs widget, the sidebar widget, the search widget, the markup-view
- Sidebar:
  - When this widget initializes, it loads its sub-iframes (rule-view, ...)
  - Each of these iframes contain panel that, in turn, listen to inspector events like "new-node-front" to know when to retrieve information about a node (i.e the rule-view will fetch the css rules for a node).
- Markup-view:
  - This panel initializes in its iframe, and gets a reference to the WalkerActor. It uses it to know the DOM tree to display. It knows when nodes change (markup-mutations), and knows what root node to start from.
  - It only displays the nodes that are supposed to be visible at first (usually html, head, body and direct children of body).
  - Then, as you expand nodes, it uses the WalkerActor to get more nodes lazily. It only ever knows data about nodes that have already been expanded once in the UI.

## Server-side structure
Simplified actor hierarchy

    InspectorActor
         |
    +---------------+
    |               |
    WalkerActor    PageStyleActor (for rule-view/computed-view)
    |               |
    NodeActor      StyleRuleActor

__InspectorActor__

This tab-level actor is the one the inspector-panel connects to. It doesn't do much apart from creating and returning the WalkerActor and PageStyleActor.

__WalkerActor__

- Single most important part of the inspector panel.
- Responsible for walking the DOM on the current page but:
  - also walks iframes
  - also finds pseudo-elements ::before and ::after
  - also finds anonymous content (e.g. in the BrowserToolbox)
- The actor uses an instance of inIDeepTreeWalker to walk the DOM
- Provides a tree of NodeActor objects that reflects the DOM.
- But only has a partial knowledge of the DOM (what is currently displayed/expanded in the MarkupView). It doesn't need to walk the whole tree when you first instantiate it.
- Reflects some of the usual DOM APIs like querySelector.
- Note that methods like querySelector return arbitrarily nested NodeActors, in which case the WalkerActor also sends the list of parents to link the returned nodes to the closest known nodes, so the UI can display the tree correctly.
- Emits events when there are DOM mutations. These events are sent to the front-end and used to, for example refresh the markup-view. This uses an instance of MutationObserver (https://developer.mozilla.org/en-US/docs/Web/API/MutationObserver) configured with, in particular, nativeAnonymousChildList set to true, so that mutation events are also sent when pseudo elements are added/removed via css.

__NodeActor__

- Representation of a single DOM node (tagname, namespace, attributes, parent, sibblings, ...), which panels use to display previews of nodes.
- Also provide useful methods to:
  - change attributes
  - scroll into view
  - get event listeners data
  - get image data
  - get unique css selector

## Highlighters

One of the important aspects of the inspector is the highlighters.
Some documentation already at: https://wiki.mozilla.org/DevTools/Highlighter

We don't just have 1 highlighter, we have a framework for highlighters:
- a (chrome-only) platform API to inject markup in a native-anonymous node in content (that works on all targets)
- a number of specific highlighter implementations (css transform, rect, selector, geometry, rulers, ...)
- a CustomHighlighterActor to get instances of specific highlighters

The entry point is toolbox-highlighter-utils.js:
- get it with toolbox.highlighterUtils
- use this to easily highlight any node in the page (with the usual box model highlighter),
- also use this to instantiate an other specific highlighter
