/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

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
  undefined setPriority(TaskPriority priority);
};
