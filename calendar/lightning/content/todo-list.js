/* */

var todoListManager = {
    mOuterBox: null
};

todoListManager.initialize =
function initialize()
{
    this.mOuterBox = document.getElementById("todo-outer-box");
    getCompositeCalendar().addObserver(this.calendarObserver);
    this.populateTodoList();
};

todoListManager.addTodo =
function addTodo(newTodo)
{
    dump("adding new todo " + newTodo.title + "\n");
    var newTodoElt = document.createElement("calendar-todo-item");
    newTodoElt.setAttribute("item-calendar", newTodo.calendar.uri.spec);

    this.mOuterBox.appendChild(newTodoElt);

    newTodoElt.setTodo(newTodo);
};

todoListManager.calendarOpListener =
{
};

todoListManager.calendarOpListener.onOperationComplete =
function listener_onOperationComplete()
{
    void("finished todo query!\n");
};

todoListManager.calendarOpListener.onGetResult =
function listener_onGetResult(calendar, status, itemtype, detail, count, items)
{
    if (!Components.isSuccessCode(status))
        return Components.reportError("get failed: " + status);
    
    void("got " + count + " todos\n");
    items.forEach(todoListManager.addTodo, todoListManager);
};

todoListManager.calendarObserver =
{
};

todoListManager.calendarObserver.onAddItem =
function observer_onAddItem(item)
{
    if (!(item instanceof Components.interfaces.calITodo))
        return;
    
    todoListManager.addTodo(item);
};

todoListManager.populateTodoList =
function populateTodoList()
{
    const cIC = Components.interfaces.calICalendar;
    var filter = cIC.ITEM_FILTER_TYPE_TODO | cIC.ITEM_FILTER_COMPLETED_ALL;
    getCompositeCalendar().getItems(filter, 0, null, null, this.calendarOpListener);
    void("started todo query!\n");
};

function initializeTodoList()
{
    todoListManager.initialize();
}

window.addEventListener("load", initializeTodoList, false);
