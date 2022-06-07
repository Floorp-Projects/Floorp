# Document Accessibility Lifecycle

## 1. DocAccessible creation
When a DocAccessible is created, it is initially empty.
A DocAccessible can be created in one of several ways:

### Scenario 1: Accessibility service is already started, layout fires an a11y notification for a new document
1. The layout [PresShell gets initialized (PresShell::Initialize)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/layout/base/PresShell.cpp#1820).
2. As part of that, layout [inserts content from the root content object down](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/layout/base/PresShell.cpp#1885).
3. This [fires an accessibility insertion notification (nsAccessibilityService::ContentRangeInserted)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/layout/base/nsCSSFrameConstructor.cpp#6863).
4. That [gets the DocAccessible (DocManager::GetDocAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/nsAccessibilityService.cpp#463).
5. Because it doesn't exist yet, [the DocAccessible gets created (DocManager::CreateDocOrRootAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#62).

### Scenario 2: Accessibility service is already started, DOM loading completes for a new document
For top level content documents, if the accessibility service is started, layout should fire an a11y notification, resulting in scenario 1 above.
For child documents (e.g. in-process iframes), this might not happen because the container Accessible for the child document might not be created yet.
In that case, we can't create a DocAccessible when the layout notification is fired.
If the container Accessible is created when DOM loading completes for the child document, this scenario can occur.

1. [a11y::DocManager gets notified that the document has stopped loading (DocManager::OnStateChange)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#238).
2. It tries to get an existing DocAccessible, but it doesn't exist yet, so it [creates a DocAccessible (DocManager::CreateDocOrRootAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#395).

### Scenario 3: Accessibility service is already started, child document is reached while building accessibility tree for parent document
Child document here refers to a child document in the same process; i.e. an in-process iframe or a parent process document such as an about: page.
Note that scenario 1 or 2 could happen for a child document as well.

1. While building the accessibility tree for the parent document, an OuterDocAccessible is created (e.g. for a XUL browser or iframe).
2. The [OuterDocAccessible constructor gets the DocAccessible for the child document (DocManager::GetDocAccessible)](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/generic/OuterDocAccessible.cpp#56).
3. Because it doesn't exist yet, [the DocAccessible gets created (DocManager::CreateDocOrRootAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#62).

### Scenario 4: Accessibility service starts after top level document is loaded
1. When the accessibility service starts, it [initializes the ApplicationAccessible (ApplicationAccessible::Init)](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/base/nsAccessibilityService.cpp#1219).
2. As part of that, all documents are iterated.
3. For each top level document, [the DocAccessible is retrieved (DocManager::GetDocAccessible)](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/generic/ApplicationAccessible.cpp#130) and [thus created (DocManager::CreateDocOrRootAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#62).

Scenario 3 would then apply for any child documents encountered while building the accessibility trees for these top level DocAccessibles.

### Scenario 5: Document gets focus before layout begins
It is possible for a document to get focus before layout has begun and before DOM loading is complete.
In that case, there will be a PresShell, but it will have no root frame.
Despite this, it is necessary to create the document because otherwise, a11y focus would go nowhere while the document had DOM focus.

1. [a11y::FocusManager gets notified of a DOM focus change (FocusManager::NotifyOfDOMFocus)](https://searchfox.org/mozilla-central/rev/04dbb1a865894aec20eb02585aa75acccc0b72d5/accessible/base/FocusManager.cpp#126).
2. It gets the DocAccessible for the child document (DocManager::GetDocAccessible).
3. Because it doesn't exist yet, [the DocAccessible gets created (DocManager::CreateDocOrRootAccessible)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#62).

## 2. Initial tree creation
1. When a DocAccessible is created, it [creates a refresh observer (NotificationController)](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/generic/DocAccessible.cpp#368) which performs various processing asynchronously.
2. When the NotificationController is created, it [schedules processing for the next possible refresh tick](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/base/NotificationController.cpp#39).
3. Once a refresh tick occurs, [while there is not an interruptible reflow in progress](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/base/NotificationController.cpp#615) and there is an initialized PresShell, the DocAccessible's [initial update is triggered (DocAccessible::DoInitialUpdate)](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#649).
4. For a top level document, the [DocAccessibleChild IPC actor is created](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/generic/DocAccessible.cpp#1752). See the section on IPC actor creation below.
5. The [DOM tree is walked and the accessibility tree is built for the document down (DocAccessible::CacheChildrenInSubtree)](https://searchfox.org/mozilla-central/rev/36f79bed679ad7ec46f7cd05868a8f6dc823e1be/accessible/generic/DocAccessible.cpp#1789).

Note that the document might still have no layout frame if the PresShell still has no frame; see scenario 5 in DocAccessible creation above.
Nevertheless, DoInitialUpdate must be called because otherwise, we wouldn't create the IPC actor, which would in turn mean remote documents in this state couldn't get a11y focus.

## 3. Child document binding
Child document here refers to a child document in the same process; e.g. an in-process iframe or a parent process document such as an about: page.

Child documents need to be a child of their OuterDocAccessible; e.g. the iframe.
However, the child document might be ready before the parent document is.
To deal with this:

1. When a DocAccessible is created for a child document (DocManager::CreateDocOrRootAccessible), it is [scheduled for binding to its parent (DocAccessible::BindChildDocument)](https://searchfox.org/mozilla-central/rev/1758450798ae14492ba28b695f48143840ad6c5b/accessible/base/DocManager.cpp#505).
2. NotificationController [does not handle any updates for child documents until they are bound to their parent](https://searchfox.org/mozilla-central/rev/1758450798ae14492ba28b695f48143840ad6c5b/accessible/base/NotificationController.cpp#638).
3. After the initial tree creation for the parent document, NotificationController [binds the document scheduled in 1)](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#783).

## 4. IPC actor (DocAccessibleChild/Parent) creation

### Scenario 1: Top level document
1. As the first part of the DocAccessible's initial update (DocAccessible::DoInitialUpdate), if the document is a top level document, it [creates a DocAccessibleChild](https://searchfox.org/mozilla-central/rev/1758450798ae14492ba28b695f48143840ad6c5b/accessible/generic/DocAccessible.cpp#1757).
    - There is also a [code path to handle the case where the DocAccessibleChild has already been created](https://searchfox.org/mozilla-central/source/accessible/generic/DocAccessible.cpp#1755). However, it doesn't seem like this should happen and code coverage information suggests that it doesn't.
2. It then [sends a message to the parent process to construct the DocAccessibleParent (BrowserChild::SendPDocAccessibleConstructor)](https://searchfox.org/mozilla-central/source/accessible/generic/DocAccessible.cpp#1771).

### Scenario 2: Child document
The [DocAccessibleChild for a child document is created](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#909) when the child document is bound to its parent by NotificationController.
There is also a [code path to handle the case where the DocAccessibleChild has already been created](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#905).
However, it doesn't seem like this should happen and code coverage information suggests that it doesn't.

## 5. Document load events

### Scenario 1: DocAccessible is already created, DOM loading completes
1. [a11y::DocManager gets notified that the document has stopped loading (DocManager::OnStateChange)](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/base/DocManager.cpp#238).
2. It [notifies the DocAccessible (DocAccessible::NotifyOfLoad](https://searchfox.org/mozilla-central/source/accessible/base/DocManager.cpp#399), passing EVENT_DOCUMENT_LOAD_COMPLETE.
3. That [sets the eDOMLoaded LoadState](https://searchfox.org/mozilla-central/rev/4e87b5392eafe1f1d49017e76f7317b06ec0b1d8/accessible/generic/DocAccessible-inl.h#93) and mLoadEventType on the DocAccessible.
4. Something schedules NotificationController processing. This could be the initial update, an insertion, etc.
5. Because the DocAccessible has been marked as loaded, the initial tree has been built and all child documents are loded, [NotificationController calls DocAccessible::ProcessLoad](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#815).
6. ProcessLoad [fires the EVENT_DOCUMENT_LOAD_COMPLETE event](https://searchfox.org/mozilla-central/source/accessible/generic/DocAccessible.cpp#1841) as set in 3).

### Scenario 2: DocAccessible is created some time after DOM loading completed
This can happen if the accessibility service is started late.
It can also happen if a DocAccessible couldn't be created earlier because the PresShell or containre Accessible wasn't created yet.

1. The [DocAccessible is initialized (DocAccessible::Init)](https://searchfox.org/mozilla-central/rev/8db61933e64b13c4a0ae456bcaccbd86a519ccc5/accessible/generic/DocAccessible.cpp#359).
2. It [detects that DOM loading is already complete](https://searchfox.org/mozilla-central/rev/8db61933e64b13c4a0ae456bcaccbd86a519ccc5/accessible/generic/DocAccessible.cpp#379).
3. In response, it [sets the eDOMLoaded state and mLoadEventType on the DocAccessible](https://searchfox.org/mozilla-central/rev/8db61933e64b13c4a0ae456bcaccbd86a519ccc5/accessible/generic/DocAccessible.cpp#381).
4. Something schedules NotificationController processing. This could be the initial update, an insertion, etc.
5. Because the DocAccessible has been marked as loaded, the initial tree has been built and all child documents are loded, [NotificationController calls DocAccessible::ProcessLoad](https://searchfox.org/mozilla-central/source/accessible/base/NotificationController.cpp#815).
6. ProcessLoad [fires the EVENT_DOCUMENT_LOAD_COMPLETE event](https://searchfox.org/mozilla-central/source/accessible/generic/DocAccessible.cpp#1841) as set in 3).

### Suppression of document load events for parent process iframes
Note that for documents loaded directly in the parent process, ProcessLoad won't fire a load event for a child document whose parent is still loading.
This is old behavior which does not work in the content process and will probably be removed in future.
See [bug 1700362](https://bugzilla.mozilla.org/show_bug.cgi?id=1700362).
