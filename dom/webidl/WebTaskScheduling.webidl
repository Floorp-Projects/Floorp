enum TaskPriority {
  "user-blocking",
  "user-visible",
  "background"
};

[Exposed=(Window, Worker), Pref="dom.enable_web_task_scheduling"]
interface TaskSignal : AbortSignal {
  readonly attribute TaskPriority priority;

  attribute EventHandler onprioritychange;
};


dictionary SchedulerPostTaskOptions {
  AbortSignal signal;
  TaskPriority priority;
  [EnforceRange] unsigned long long delay = 0;
};

callback SchedulerPostTaskCallback = any ();

[Exposed=(Window, Worker), Pref="dom.enable_web_task_scheduling"]
interface Scheduler {
  [UseCounter]
  Promise<any> postTask(
    SchedulerPostTaskCallback callback,
    optional SchedulerPostTaskOptions options = {}
  );
};

dictionary TaskControllerInit {
  TaskPriority priority = "user-visible";
};

[Exposed=(Window,Worker), Pref="dom.enable_web_task_scheduling"]
interface TaskController : AbortController {
  [Throws]
  constructor(optional TaskControllerInit init = {});

  [Throws]
  void setPriority(TaskPriority priority);
};
