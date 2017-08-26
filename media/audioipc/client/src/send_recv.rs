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
    (__send $conn:expr, $smsg:ident) => (
        $conn.send(ServerMessage::$smsg)
            .unwrap();
    );
    (__send $conn:expr, $smsg:ident, $($a:expr),*) => (
        $conn.send(ServerMessage::$smsg($($a),*))
            .unwrap();
    );
    (__recv $conn:expr, $rmsg:ident) => (
        if let ClientMessage::$rmsg = $conn.receive().unwrap() {
            Ok(())
        } else {
            panic!("wrong message received");
        }
    );
    (__recv $conn:expr, $rmsg:ident __result) => (
        if let ClientMessage::$rmsg(v) = $conn.receive().unwrap() {
            Ok(v)
        } else {
            panic!("wrong message received");
        }
    )
}
