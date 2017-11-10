from mod_pywebsocket import msgutil
from mod_pywebsocket import common

def web_socket_do_extra_handshake(request):
  pmce_offered = False

  if request.ws_requested_extensions is not None:
    for extension_request in request.ws_requested_extensions:
      if extension_request.name() == "permessage-deflate":
        pmce_offered = True

  if pmce_offered is False:
    raise ValueError('permessage-deflate not offered')

def web_socket_transfer_data(request):
  while True:
    rcvd = msgutil.receive_message(request)
    opcode = request.ws_stream.get_last_received_opcode()
    if (opcode == common.OPCODE_BINARY):
      msgutil.send_message(request, rcvd, binary=True)
    elif (opcode == common.OPCODE_TEXT):
      msgutil.send_message(request, rcvd)
