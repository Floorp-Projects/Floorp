#[macro_export]
macro_rules! send_recv {
    ($conn:expr, $smsg:ident => $rmsg:ident) => {{
        send_recv!(__send $conn, $smsg);
        send_recv!(__recv $conn, $rmsg)
    }};
    ($conn:expr, $smsg:ident => $rmsg:ident()) => {{
        send_recv!(__send $conn, $smsg);
        send_recv!(__recv $conn, $rmsg __result)
    }};
    ($conn:expr, $smsg:ident($($a:expr),*) => $rmsg:ident) => {{
        send_recv!(__send $conn, $smsg, $($a),*);
        send_recv!(__recv $conn, $rmsg)
    }};
    ($conn:expr, $smsg:ident($($a:expr),*) => $rmsg:ident()) => {{
        send_recv!(__send $conn, $smsg, $($a),*);
        send_recv!(__recv $conn, $rmsg __result)
    }};
    //
    (__send $conn:expr, $smsg:ident) => ({
        let r = $conn.send(ServerMessage::$smsg);
        if r.is_err() {
            debug!("send error - got={:?}", r);
            return Err(ErrorCode::Error.into());
        }
    });
    (__send $conn:expr, $smsg:ident, $($a:expr),*) => ({
        let r = $conn.send(ServerMessage::$smsg($($a),*));
        if r.is_err() {
            debug!("send error - got={:?}", r);
            return Err(ErrorCode::Error.into());
        }
    });
    (__recv $conn:expr, $rmsg:ident) => ({
        match $conn.receive() {
            Ok(ClientMessage::$rmsg) => Ok(()),
            // Macro can see ErrorCode but not Error? I don't understand.
            // ::cubeb_core::Error is fragile but this isn't general purpose code.
            Ok(ClientMessage::Error(e)) => Err(unsafe { ::cubeb_core::Error::from_raw(e) }),
            Ok(m) => {
                debug!("receive unexpected message - got={:?}", m);
                Err(ErrorCode::Error.into())
            },
            Err(e) => {
                debug!("receive error - got={:?}", e);
                Err(ErrorCode::Error.into())
            },
        }
    });
    (__recv $conn:expr, $rmsg:ident __result) => ({
        match $conn.receive() {
            Ok(ClientMessage::$rmsg(v)) => Ok(v),
            // Macro can see ErrorCode but not Error? I don't understand.
            // ::cubeb_core::Error is fragile but this isn't general purpose code.
            Ok(ClientMessage::Error(e)) => Err(unsafe { ::cubeb_core::Error::from_raw(e) }),
            Ok(m) => {
                debug!("receive unexpected message - got={:?}", m);
                Err(ErrorCode::Error.into())
            },
            Err(e) => {
                debug!("receive error - got={:?}", e);
                Err(ErrorCode::Error.into())
            },
        }
    })
}
