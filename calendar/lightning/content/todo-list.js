function eventToTodo(event)
{
    try {
        return event.originalTarget.selectedItem.todo;
    } catch (e) {
        return null;
    }
}

function handleTodoListEvent(event)
{
    var todo = eventToTodo(event);
    var name = todo ? ('"' + todo.title + '"') : "<none>";
    dump("Todo list event: " + event.type + " (" + name + ")\n");
}

function initializeTodoList()
{
    var todoList = document.getElementById("calendar-todo-list");
    todoList.calendar = getCompositeCalendar();
    todoList.addEventListener("todo-item-open", handleTodoListEvent, false);
    todoList.addEventListener("todo-item-delete", handleTodoListEvent, false);
    todoList.addEventListener("todo-empty-dblclick", handleTodoListEvent, false);
}

window.addEventListener("load", initializeTodoList, false);
