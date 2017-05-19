# Architecture overview 

Broadly speaking, the tools are divided in two parts: the server and the client. A **server** is anything that can be debugged: for example, your browser, but it could also be Firefox for Android, running on another device. The **client** is the front-end side of the tools, and it is what developers interact with when using the tools.

Since these two parts are decoupled, we can connect to any server using the same client. This enables us to debug multiple types of servers, using the same protocol to communicate.

You will often hear about `actors`. Each feature that can be debugged (for example, network) is exposed via an `actor`, which provides data about that specific feature. It's up to each server to implement some or all actors; the client needs to find out and decide what it can render on the front-side when it connects to the server. So when we want to debug a new feature, we might need to do work in two parts of the code: the server (perhaps implementing a new actor, or extending existing ones) and the client (to display the debugging data returned by the actor).

Often, an actor will correspond to a panel. But a panel might want to get data from multiple actors.

You might also hear about `the toolbox`. The toolbox is what everyone else calls `developer tools` i.e. the front-end that you see when you open the tools in your browser.


