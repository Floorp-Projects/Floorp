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
        let r = $conn.receive().unwrap();
        if let ClientMessage::$rmsg = r {
            Ok(())
        } else {
            debug!("receive error - got={:?}", r);
            Err(ErrorCode::Error.into())
        }
    });
    (__recv $conn:expr, $rmsg:ident __result) => ({
        let r = $conn.receive().unwrap();
        if let ClientMessage::$rmsg(v) = r {
            Ok(v)
        } else {
            debug!("receive error - got={:?}", r);
            Err(ErrorCode::Error.into())
        }
    })
}
