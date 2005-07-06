function initializeTodoList()
{
    var todoList = document.getElementById("calendar-todo-list");
    todoList.calendar = getCompositeCalendar();
}

window.addEventListener("load", initializeTodoList, false);
