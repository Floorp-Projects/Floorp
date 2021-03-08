from mod_pywebsocket import msgutil
import six


def web_socket_do_extra_handshake(request):
    pass  # Always accept.


def web_socket_transfer_data(request):
    expected_messages = ["Hello, world!", "", all_distinct_bytes()]

    for test_number, expected_message in enumerate(expected_messages):
        expected_message = six.b(expected_message)
        message = msgutil.receive_message(request)
        if message == expected_message:
            msgutil.send_message(request, "PASS: Message #{:d}.".format(test_number))
        else:
            msgutil.send_message(
                request,
                "FAIL: Message #{:d}: Received unexpected message: {!r}".format(
                    test_number, message
                ),
            )


def all_distinct_bytes():
    return "".join([chr(i) for i in range(256)])
