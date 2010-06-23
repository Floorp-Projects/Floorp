from mod_pywebsocket import msgutil

import time
import sys

# see the list of tests in test_websocket.html

def web_socket_do_extra_handshake(request):
  if request.ws_protocol == "test 6":
    sys.exit(0)
  elif request.ws_protocol == "test 19":
    time.sleep(180)
    pass
  elif request.ws_protocol == "test 8":
    time.sleep(5)
    pass
  elif request.ws_protocol == "test 9":
    time.sleep(5)
    pass
  elif request.ws_protocol == "test 10.1":
    time.sleep(5)
    pass
  else:
    pass

def web_socket_transfer_data(request):
  if request.ws_protocol == "test 9":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 11":
    resp = "wrong message"
    if msgutil.receive_message(request) == "client data":
      resp = "server data"
    msgutil.send_message(request, resp.decode('utf-8'))
  elif request.ws_protocol == "test 13":
    # first one binary message containing the byte 0x61 ('a')
    request.connection.write('\xff\x01\x61')
    # after a bad utf8 message
    request.connection.write('\x01\x61\xff')
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 14":
    request.connection.write('\xff\x00')
    msgutil.send_message(request, "server data")
  elif request.ws_protocol == "test 15":
    sys.exit (0)
  elif request.ws_protocol == "test 17":
    while not request.client_terminated:
      msgutil.send_message(request, "server data")
      time.sleep(1)
    msgutil.send_message(request, "server data")
    sys.exit(0)
  elif request.ws_protocol == "test 18":
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
  elif request.ws_protocol == "test 10.1" or request.ws_protocol == "test 10.2":
    msgutil.close_connection(request)
  while not request.client_terminated:
    msgutil.receive_message(request)
