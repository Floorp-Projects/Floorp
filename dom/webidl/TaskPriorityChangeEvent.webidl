/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

[Exposed=(Window, Worker), Pref="dom.enable_web_task_scheduling"]
interface TaskPriorityChangeEvent : Event {
  constructor (DOMString type , TaskPriorityChangeEventInit priorityChangeEventInitDict);
  readonly attribute TaskPriority previousPriority;
};

dictionary TaskPriorityChangeEventInit : EventInit {
  required TaskPriority previousPriority;
};
