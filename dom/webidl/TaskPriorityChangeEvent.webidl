[Exposed=(Window, Worker), Pref="dom.enable_web_task_scheduling"]
interface TaskPriorityChangeEvent : Event {
  constructor (DOMString type , TaskPriorityChangeEventInit priorityChangeEventInitDict);
  readonly attribute TaskPriority previousPriority;
};

dictionary TaskPriorityChangeEventInit : EventInit {
  required TaskPriority previousPriority;
};
