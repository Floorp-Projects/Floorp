function eventToTodo(event)
{
    try {
        return event.originalTarget.selectedItem.todo;
    } catch (e) {
        return null;
    }
}

function editTodoItem(event)
{
    var todo = eventToTodo(event);
    if (todo)
        modifyEventWithDialog(todo);
}

function newTodoItem(event)
{
    createTodoWithDialog(ltnSelectedCalendar());
}

function deleteTodoItem(event)
{
    var todo = eventToTodo(event);
    if (todo)
        todo.calendar.deleteItem(todo, null);
}

function initializeTodoList()
{
    var todoList = document.getElementById("calendar-todo-list");
    todoList.calendar = getCompositeCalendar();
    todoList.addEventListener("todo-item-open", editTodoItem, false);
    todoList.addEventListener("todo-item-delete", deleteTodoItem, false);
    todoList.addEventListener("todo-empty-dblclick", newTodoItem, false);
}

window.addEventListener("load", initializeTodoList, false);
