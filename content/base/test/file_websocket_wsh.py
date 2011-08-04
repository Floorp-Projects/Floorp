from mod_pywebsocket import msgutil

import time
import sys

# see the list of tests in test_websocket.html

def web_socket_do_extra_handshake(request):
  # must set request.ws_protocol to the selected version from ws_requested_protocols
  for x in request.ws_requested_protocols:
    if x != "test-does-not-exist":
      request.ws_protocol = x
      break

  if request.ws_protocol == "test-2.1":
    time.sleep(3)
    pass
  elif request.ws_protocol == "test-9":
    time.sleep(3)
    pass
  elif request.ws_protocol == "test-10":
    time.sleep(3)
    pass
  elif request.ws_protocol == "test-19":
    raise ValueError('Aborting (test-19)')
  elif request.ws_protocol == "test-20" or request.ws_protocol == "test-17":
    time.sleep(3)
    pass
  elif request.ws_protocol == "test-22":
    # The timeout is 5 seconds
    time.sleep(13)
    pass
  else:
    pass

def web_socket_transfer_data(request):
  if request.ws_protocol == "test-2.1" or request.ws_protocol == "test-2.2":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-6":
    resp = "wrong message"
    if msgutil.receive_message(request) == "1":
      resp = "2"
    msgutil.send_message(request, resp.decode('utf-8'))
    resp = "wrong message"
    if msgutil.receive_message(request) == "3":
      resp = "4"
    msgutil.send_message(request, resp.decode('utf-8'))
    resp = "wrong message"
    if msgutil.receive_message(request) == "5":
      resp = "あいうえお"
    msgutil.send_message(request, resp.decode('utf-8'))
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-7":
    msgutil.send_message(request, "test-7 data")
  elif request.ws_protocol == "test-10":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-11":
    resp = "wrong message"
    if msgutil.receive_message(request) == "client data":
      resp = "server data"
    msgutil.send_message(request, resp.decode('utf-8'))
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-12":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-13":
    # first one binary message containing the byte 0x61 ('a')
    request.connection.write('\xff\x01\x61')
    # after a bad utf8 message
    request.connection.write('\x01\x61\xff')
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-14":
    msgutil.close_connection(request)
    msgutil.send_message(request, "server data")
  elif request.ws_protocol == "test-15":
    msgutil.close_connection(request, True)
    return
  elif request.ws_protocol == "test-17" or request.ws_protocol == "test-21":
    time.sleep(2)
    resp = "wrong message"
    if msgutil.receive_message(request) == "client data":
      resp = "server data"
    msgutil.send_message(request, resp.decode('utf-8'))
    time.sleep(2)
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-20":
    msgutil.send_message(request, "server data")
    msgutil.close_connection(request)
  elif request.ws_protocol == "test-34":
    request.ws_stream.close_connection(1001, "going away now")
  elif request.ws_protocol == "test-35a":
    while not request.client_terminated:
      msgutil.receive_message(request)
    global test35code
    test35code = request.ws_close_code
    global test35reason
    test35reason = request.ws_close_reason
  elif request.ws_protocol == "test-35b":
    request.ws_stream.close_connection(test35code + 1, test35reason)
  elif request.ws_protocol == "test-37b":
    while not request.client_terminated:
      msgutil.receive_message(request)
    global test37code
    test37code = request.ws_close_code
    global test37reason
    test37reason = request.ws_close_reason
  elif request.ws_protocol == "test-37c":
    request.ws_stream.close_connection(test37code, test37reason)
  while not request.client_terminated:
    msgutil.receive_message(request)
