1. ServiceWorkerRegistrar loading. The ability to determine whether to intercept
   is based on this. (Although if not loaded, it's possible to just not intercept.)

2. Process launching. ServiceWorkers need to be launched into a process, and
   under fission this will almost certainly be a new process, and at startup we
   might not be able to depend on preallocated processes.

3. Permission transmission.

4. Worker launching. The act of spawning the worker thread in the content process.

5. Script loading.
        Cache API opening for the given origin.
        QuotaManager storage and temporary storage initialization, which has to
        happen before the Cache API can start accessing its files.

6. Fetch request serialization / deserialization to parent process.

7. InterceptedHttpChannel creation for the fetch request.

8. Creating FetchEvent related objects and propagting to the content process
   worker thread.

9. Handle FetchEvent by the ServiceWorker's script.

10. Propagating the response from ServiceWorker to parent process.

11. Synthesizing the response for the intercepted channel.

12. Reset the interception by redirecting to a normal http channel or cancel the
    interception.

13. Push data into the intercepted channel.

Telemetry probes cover:

1: SERVICE_WORKER_REGISTRATION_LOADING
2-4: SERVICE_WORKER_LAUNCH_TIME_2
2-5, 7-13: SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS_2
7-9: SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS_2
11: SERVICE_WORKER_FETCH_EVENT_FINISH_SYNTHESIZED_RESPONSE_MS_2
12: SERVICE_WORKER_FETCH_EVENT_CHANNEL_RESET_MS_2
